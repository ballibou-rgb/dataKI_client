#include "api.h"

#include <string.h>
#include <gio/gio.h>

#include "http.h"

typedef struct
{
  char             *server_url;
  char             *device_key;
  char             *path;
  DatakiApiCallback callback;
  gpointer          user_data;
  /* result */
  long              status;
  char             *body;
} ApiCall;

static char *
join_url(const char *server_url, const char *path)
{
  g_autofree char *base = g_strdup(server_url);
  gsize len = strlen(base);
  if (len > 0 && base[len - 1] == '/')
    base[len - 1] = '\0';
  return g_strconcat(base, path, NULL);
}

static void
api_thread(GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
  (void)source; (void)cancellable;
  ApiCall *c = task_data;

  g_autofree char *url = join_url(c->server_url, c->path);
  DatakiHttpResponse resp = {0};
  GError *error = NULL;
  if (!dataki_http_get(url, c->device_key, &resp, &error))
    {
      g_task_return_error(task, error);
      return;
    }
  c->status = resp.status;
  c->body   = resp.body;  /* transfer */
  resp.body = NULL;
  dataki_http_response_clear(&resp);
  g_task_return_boolean(task, TRUE);
}

static void
api_done(GObject *source, GAsyncResult *result, gpointer user_data)
{
  (void)source;
  ApiCall *c = user_data;
  GError *error = NULL;
  gboolean ok = g_task_propagate_boolean(G_TASK(result), &error);

  if (!ok)
    c->callback(FALSE, 0, NULL, c->user_data);
  else
    c->callback(c->status == 200, c->status, c->status == 200 ? c->body : NULL, c->user_data);

  g_clear_error(&error);
  g_free(c->server_url);
  g_free(c->device_key);
  g_free(c->path);
  g_free(c->body);
  g_free(c);
}

void
dataki_api_get_async(const char        *server_url,
                     const char        *device_key,
                     const char        *path,
                     DatakiApiCallback  callback,
                     gpointer           user_data)
{
  ApiCall *c = g_new0(ApiCall, 1);
  c->server_url = g_strdup(server_url);
  c->device_key = g_strdup(device_key);
  c->path       = g_strdup(path);
  c->callback   = callback;
  c->user_data  = user_data;

  GTask *task = g_task_new(NULL, NULL, api_done, c);
  g_task_set_task_data(task, c, NULL);
  g_task_run_in_thread(task, api_thread);
  g_object_unref(task);
}
