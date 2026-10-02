#include "login.h"

#include <string.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "dataki-config.h"
#include "http.h"
#include "loopback.h"
#include "secret.h"

#define LOGIN_TIMEOUT_SECONDS 300
#define SECRET_ACCOUNT        "default"

struct _DatakiLogin
{
  char                   *server_url;
  char                   *device_name;
  char                   *state;
  DatakiLoopback         *loopback;
  guint                   timeout_id;
  DatakiLoginDoneCallback callback;
  gpointer                user_data;
  gboolean                finished;
};

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static char *
server_endpoint(const char *server_url, const char *path)
{
  g_autofree char *base = g_strdup(server_url);
  gsize len = strlen(base);
  if (len > 0 && base[len - 1] == '/')
    base[len - 1] = '\0';
  return g_strconcat(base, path, NULL);
}

static char *
device_name_default(void)
{
  return g_strdup_printf("Desktop · %s", g_get_host_name());
}

/* Extract a top-level string member from a JSON object body. */
static char *
json_string_member(const char *body, const char *member)
{
  g_autoptr(JsonParser) parser = json_parser_new();
  if (!json_parser_load_from_data(parser, body, -1, NULL))
    return NULL;
  JsonNode *root = json_parser_get_root(parser);
  if (root == NULL || !JSON_NODE_HOLDS_OBJECT(root))
    return NULL;
  JsonObject *obj = json_node_get_object(root);
  if (!json_object_has_member(obj, member))
    return NULL;
  return g_strdup(json_object_get_string_member(obj, member));
}

/* -------------------------------------------------------------------------- */
/* Exchange + bootstrap worker                                                */
/* -------------------------------------------------------------------------- */

typedef struct
{
  char *server_url;
  char *device_name;
  char *code;
} ExchangeData;

static void
exchange_data_free(gpointer p)
{
  ExchangeData *d = p;
  g_free(d->server_url);
  g_free(d->device_name);
  g_free(d->code);
  g_free(d);
}

static void
exchange_thread(GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
  (void)source;
  (void)cancellable;
  ExchangeData *d = task_data;
  GError *error = NULL;

  /* 1. exchange code -> device key */
  g_autofree char *ex_url = server_endpoint(d->server_url, "/api/v1/client/login/exchange.php");
  g_autoptr(JsonBuilder) b = json_builder_new();
  json_builder_begin_object(b);
  json_builder_set_member_name(b, "code");            json_builder_add_string_value(b, d->code);
  json_builder_set_member_name(b, "device_name");     json_builder_add_string_value(b, d->device_name);
  json_builder_set_member_name(b, "client_version");  json_builder_add_string_value(b, DATAKI_VERSION);
  json_builder_end_object(b);
  g_autoptr(JsonGenerator) gen = json_generator_new();
  json_generator_set_root(gen, json_builder_get_root(b));
  g_autofree char *req_body = json_generator_to_data(gen, NULL);

  DatakiHttpResponse ex = {0};
  if (!dataki_http_post_json(ex_url, NULL, req_body, &ex, &error))
    {
      g_task_return_error(task, error);
      return;
    }

  g_autofree char *status = json_string_member(ex.body, "status");
  g_autofree char *device_key = json_string_member(ex.body, "device_key");
  if (ex.status != 200 || g_strcmp0(status, "ok") != 0 || device_key == NULL)
    {
      g_autofree char *msg = json_string_member(ex.body, "error");
      dataki_http_response_clear(&ex);
      g_task_return_new_error(task, g_quark_from_static_string("dataki-login"), 1,
                              "Anmeldung fehlgeschlagen: %s",
                              msg ? msg : "ungültiger oder abgelaufener Code");
      return;
    }
  dataki_http_response_clear(&ex);

  /* 2. store the device key */
  if (!dataki_secret_store(d->server_url, SECRET_ACCOUNT, device_key, &error))
    {
      /* Non-fatal for the session, but the key won't persist. Log and continue. */
      g_warning("Could not store device key: %s", error ? error->message : "?");
      g_clear_error(&error);
    }

  /* 3. bootstrap */
  g_autofree char *bs_url = server_endpoint(d->server_url, "/api/v1/client/bootstrap.php");
  DatakiHttpResponse bs = {0};
  if (!dataki_http_get(bs_url, device_key, &bs, &error))
    {
      g_task_return_error(task, error);
      return;
    }
  if (bs.status != 200)
    {
      long st = bs.status;
      dataki_http_response_clear(&bs);
      g_task_return_new_error(task, g_quark_from_static_string("dataki-login"), 2,
                              "Bootstrap fehlgeschlagen (HTTP %ld).", st);
      return;
    }

  g_task_return_pointer(task, bs.body, g_free); /* hand over bootstrap body */
  bs.body = NULL;
}

static void
exchange_done(GObject *source, GAsyncResult *result, gpointer user_data)
{
  (void)source;
  DatakiLogin *self = user_data;
  GError *error = NULL;
  char *bootstrap = g_task_propagate_pointer(G_TASK(result), &error);

  if (!self->finished)
    {
      self->finished = TRUE;
      if (error != NULL)
        self->callback(FALSE, error->message, NULL, self->user_data);
      else
        self->callback(TRUE, NULL, bootstrap, self->user_data);
    }

  g_clear_error(&error);
  g_free(bootstrap);
}

/* -------------------------------------------------------------------------- */
/* Loopback + timeout                                                         */
/* -------------------------------------------------------------------------- */

static void
finish_error(DatakiLogin *self, const char *message)
{
  if (self->finished)
    return;
  self->finished = TRUE;
  if (self->timeout_id != 0)
    {
      g_source_remove(self->timeout_id);
      self->timeout_id = 0;
    }
  self->callback(FALSE, message, NULL, self->user_data);
}

static gboolean
on_timeout(gpointer user_data)
{
  DatakiLogin *self = user_data;
  self->timeout_id = 0;
  finish_error(self, "Zeitüberschreitung bei der Anmeldung.");
  return G_SOURCE_REMOVE;
}

static void
on_callback(const char *code, const char *state, gpointer user_data)
{
  DatakiLogin *self = user_data;

  if (self->finished)
    return;

  if (self->timeout_id != 0)
    {
      g_source_remove(self->timeout_id);
      self->timeout_id = 0;
    }

  if (g_getenv("DATAKI_DEBUG") != NULL)
    g_message("loopback callback: code=%s state_match=%d",
              code ? "present" : "none", g_strcmp0(state, self->state) == 0);

  if (code == NULL || *code == '\0' || g_strcmp0(state, self->state) != 0)
    {
      finish_error(self, "Ungültige Antwort vom Browser (State-Mismatch).");
      return;
    }

  ExchangeData *d = g_new0(ExchangeData, 1);
  d->server_url  = g_strdup(self->server_url);
  d->device_name = g_strdup(self->device_name);
  d->code        = g_strdup(code);

  GTask *task = g_task_new(NULL, NULL, exchange_done, self);
  g_task_set_task_data(task, d, exchange_data_free);
  g_task_run_in_thread(task, exchange_thread);
  g_object_unref(task);
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

DatakiLogin *
dataki_login_start(const char              *server_url,
                   DatakiLoginDoneCallback  callback,
                   gpointer                 user_data,
                   GError                 **error)
{
  g_return_val_if_fail(server_url != NULL, NULL);
  g_return_val_if_fail(callback != NULL, NULL);

  DatakiLogin *self = g_new0(DatakiLogin, 1);
  self->server_url  = g_strdup(server_url);
  self->device_name = device_name_default();
  self->state       = g_uuid_string_random();
  self->callback    = callback;
  self->user_data   = user_data;

  self->loopback = dataki_loopback_new(on_callback, self, error);
  if (self->loopback == NULL)
    {
      dataki_login_cancel(self);
      return NULL;
    }

  guint16 port = dataki_loopback_port(self->loopback);

  g_autofree char *redirect = g_strdup_printf("http://127.0.0.1:%u/callback", port);
  g_autofree char *redirect_esc = g_uri_escape_string(redirect, NULL, FALSE);
  g_autofree char *state_esc    = g_uri_escape_string(self->state, NULL, FALSE);
  g_autofree char *dname_esc    = g_uri_escape_string(self->device_name, NULL, FALSE);

  g_autofree char *start = server_endpoint(self->server_url, "/api/v1/client/login/start.php");
  g_autofree char *url = g_strdup_printf(
      "%s?redirect_uri=%s&state=%s&device_name=%s&client_version=%s",
      start, redirect_esc, state_esc, dname_esc, DATAKI_VERSION);

  if (g_getenv("DATAKI_DEBUG") != NULL)
    g_message("login start url: %s", url);

  g_autoptr(GError) open_error = NULL;
  if (!g_app_info_launch_default_for_uri(url, NULL, &open_error))
    g_warning("Could not open browser: %s", open_error->message);

  self->timeout_id = g_timeout_add_seconds(LOGIN_TIMEOUT_SECONDS, on_timeout, self);

  return self;
}

void
dataki_login_cancel(DatakiLogin *self)
{
  if (self == NULL)
    return;
  if (self->timeout_id != 0)
    g_source_remove(self->timeout_id);
  g_clear_pointer(&self->loopback, dataki_loopback_free);
  g_free(self->server_url);
  g_free(self->device_name);
  g_free(self->state);
  g_free(self);
}

char *
dataki_login_stored_key(const char *server_url)
{
  g_autoptr(GError) error = NULL;
  char *key = dataki_secret_lookup(server_url, SECRET_ACCOUNT, &error);
  if (error != NULL)
    g_warning("Keyring lookup failed: %s", error->message);
  return key;
}

void
dataki_login_forget_key(const char *server_url)
{
  g_autoptr(GError) error = NULL;
  dataki_secret_clear(server_url, SECRET_ACCOUNT, &error);
}

/* ---- startup bootstrap ---- */

typedef struct
{
  char                   *server_url;
  char                   *device_key;
  DatakiBootstrapCallback callback;
  gpointer                user_data;
} BootstrapData;

static void
bootstrap_thread(GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
  (void)source;
  (void)cancellable;
  BootstrapData *d = task_data;
  GError *error = NULL;

  g_autofree char *url = server_endpoint(d->server_url, "/api/v1/client/bootstrap.php");
  DatakiHttpResponse resp = {0};
  if (!dataki_http_get(url, d->device_key, &resp, &error))
    {
      g_task_return_error(task, error);
      return;
    }

  /* Pack status into the pointer result alongside the body. */
  JsonObject *wrap = json_object_new();
  json_object_set_int_member(wrap, "__status", resp.status);
  json_object_set_string_member(wrap, "__body", resp.body ? resp.body : "");
  dataki_http_response_clear(&resp);

  g_task_return_pointer(task, wrap, (GDestroyNotify)json_object_unref);
}

static void
bootstrap_done(GObject *source, GAsyncResult *result, gpointer user_data)
{
  (void)source;
  BootstrapData *d = user_data;
  GError *error = NULL;
  JsonObject *wrap = g_task_propagate_pointer(G_TASK(result), &error);

  if (error != NULL)
    {
      d->callback(FALSE, 0, NULL, d->user_data);
      g_error_free(error);
    }
  else
    {
      long status = (long)json_object_get_int_member(wrap, "__status");
      const char *body = json_object_get_string_member(wrap, "__body");
      d->callback(status == 200, status, status == 200 ? body : NULL, d->user_data);
      json_object_unref(wrap);
    }

  g_free(d->server_url);
  g_free(d->device_key);
  g_free(d);
}

void
dataki_login_bootstrap_async(const char              *server_url,
                             const char              *device_key,
                             DatakiBootstrapCallback  callback,
                             gpointer                 user_data)
{
  BootstrapData *d = g_new0(BootstrapData, 1);
  d->server_url = g_strdup(server_url);
  d->device_key = g_strdup(device_key);
  d->callback   = callback;
  d->user_data  = user_data;

  GTask *task = g_task_new(NULL, NULL, bootstrap_done, d);
  g_task_set_task_data(task, d, NULL); /* same struct; freed in bootstrap_done */
  g_task_run_in_thread(task, bootstrap_thread);
  g_object_unref(task);
}

/* ---- logout ---- */

typedef struct { char *server_url; char *device_key; } LogoutData;

static void
logout_thread(GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
  (void)source;
  (void)cancellable;
  LogoutData *d = task_data;
  g_autofree char *url = server_endpoint(d->server_url, "/api/v1/client/logout.php");
  DatakiHttpResponse resp = {0};
  GError *error = NULL;
  if (dataki_http_post_json(url, d->device_key, "{}", &resp, &error))
    dataki_http_response_clear(&resp);
  g_clear_error(&error);
  g_task_return_boolean(task, TRUE);
}

static void
logout_data_free(gpointer p)
{
  LogoutData *d = p;
  g_free(d->server_url);
  g_free(d->device_key);
  g_free(d);
}

void
dataki_login_logout_async(const char *server_url, const char *device_key)
{
  dataki_login_forget_key(server_url);

  if (device_key == NULL || *device_key == '\0')
    return;

  LogoutData *d = g_new0(LogoutData, 1);
  d->server_url = g_strdup(server_url);
  d->device_key = g_strdup(device_key);

  GTask *task = g_task_new(NULL, NULL, NULL, NULL);
  g_task_set_task_data(task, d, logout_data_free);
  g_task_run_in_thread(task, logout_thread);
  g_object_unref(task);
}
