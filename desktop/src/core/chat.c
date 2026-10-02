#include "chat.h"

#include <string.h>
#include <curl/curl.h>

#include "dataki-config.h"
#include "sse.h"

struct _DatakiChatRequest
{
  GThread *thread;

  char    *url;
  char    *bearer;

  /* params (owned copies) */
  char    *content;
  gint64   chat_id;
  char    *model;
  char    *effort;
  gboolean thinking;
  gboolean websearch;

  volatile gint cancel;

  DatakiChatEventCallback on_event;
  DatakiChatDoneCallback  on_done;
  gpointer                user_data;

  DatakiSse *sse;
};

/* ---- marshaling worker → main thread ---- */

typedef struct { DatakiChatRequest *req; char *payload; } EventMsg;

static gboolean
deliver_event(gpointer data)
{
  EventMsg *m = data;
  if (g_atomic_int_get(&m->req->cancel) == 0 && m->req->on_event != NULL)
    m->req->on_event(m->payload, m->req->user_data);
  g_free(m->payload);
  g_free(m);
  return G_SOURCE_REMOVE;
}

typedef struct { DatakiChatRequest *req; char *error; } DoneMsg;

static gboolean
deliver_done(gpointer data)
{
  DoneMsg *m = data;
  if (m->req->on_done != NULL)
    m->req->on_done(m->error, m->req->user_data);
  g_free(m->error);
  g_free(m);
  return G_SOURCE_REMOVE;
}

/* ---- SSE payload from the worker: hand to the main thread ---- */

static void
on_sse_payload(const char *payload, gpointer user_data)
{
  DatakiChatRequest *req = user_data;
  EventMsg *m = g_new0(EventMsg, 1);
  m->req = req;
  m->payload = g_strdup(payload);
  g_idle_add(deliver_event, m);
}

static size_t
write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  DatakiChatRequest *req = userdata;
  gsize total = size * nmemb;
  dataki_sse_feed(req->sse, ptr, total);
  return total;
}

static int
xfer_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
        curl_off_t ultotal, curl_off_t ulnow)
{
  (void)dltotal; (void)dlnow; (void)ultotal; (void)ulnow;
  DatakiChatRequest *req = clientp;
  return g_atomic_int_get(&req->cancel) != 0 ? 1 : 0; /* nonzero aborts */
}

/* ---- worker ---- */

static gpointer
chat_thread(gpointer data)
{
  DatakiChatRequest *req = data;
  char *error_msg = NULL;

  CURL *curl = curl_easy_init();
  if (curl == NULL)
    {
      error_msg = g_strdup("curl init failed");
      DoneMsg *m = g_new0(DoneMsg, 1);
      m->req = req;
      m->error = error_msg;
      g_idle_add(deliver_done, m);
      return NULL;
    }

  curl_mime *mime = curl_mime_init(curl);
  curl_mimepart *part;

  part = curl_mime_addpart(mime); curl_mime_name(part, "action");
  curl_mime_data(part, "send_message", CURL_ZERO_TERMINATED);
  part = curl_mime_addpart(mime); curl_mime_name(part, "content");
  curl_mime_data(part, req->content ? req->content : "", CURL_ZERO_TERMINATED);

  g_autofree char *chat_id_s = g_strdup_printf("%" G_GINT64_FORMAT, req->chat_id);
  part = curl_mime_addpart(mime); curl_mime_name(part, "chat_id");
  curl_mime_data(part, chat_id_s, CURL_ZERO_TERMINATED);

  part = curl_mime_addpart(mime); curl_mime_name(part, "model");
  curl_mime_data(part, req->model ? req->model : "", CURL_ZERO_TERMINATED);

  if (req->effort != NULL)
    {
      part = curl_mime_addpart(mime); curl_mime_name(part, "effort");
      curl_mime_data(part, req->effort, CURL_ZERO_TERMINATED);
    }

  part = curl_mime_addpart(mime); curl_mime_name(part, "thinking");
  curl_mime_data(part, req->thinking ? "1" : "0", CURL_ZERO_TERMINATED);
  part = curl_mime_addpart(mime); curl_mime_name(part, "websearch");
  curl_mime_data(part, req->websearch ? "1" : "0", CURL_ZERO_TERMINATED);

  struct curl_slist *headers = NULL;
  g_autofree char *auth = g_strdup_printf("Authorization: Bearer %s", req->bearer);
  headers = curl_slist_append(headers, auth);
  headers = curl_slist_append(headers, "X-Client: dataKI-native/" DATAKI_VERSION);
  headers = curl_slist_append(headers, "Accept: text/event-stream");

  curl_easy_setopt(curl, CURLOPT_URL, req->url);
  curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, req);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfer_cb);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, req);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 120L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  CURLcode rc = curl_easy_perform(curl);
  dataki_sse_flush(req->sse);

  if (rc != CURLE_OK && rc != CURLE_ABORTED_BY_CALLBACK &&
      g_atomic_int_get(&req->cancel) == 0)
    {
      long status = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
      if (status >= 400)
        error_msg = g_strdup_printf("Serverfehler (HTTP %ld).", status);
      else
        error_msg = g_strdup(curl_easy_strerror(rc));
    }

  curl_mime_free(mime);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  DoneMsg *m = g_new0(DoneMsg, 1);
  m->req = req;
  m->error = error_msg; /* transferred */
  g_idle_add(deliver_done, m);
  return NULL;
}

/* ---- public ---- */

DatakiChatRequest *
dataki_chat_send(const char              *server_url,
                 const char              *device_key,
                 const DatakiChatParams  *params,
                 DatakiChatEventCallback  on_event,
                 DatakiChatDoneCallback   on_done,
                 gpointer                 user_data)
{
  g_return_val_if_fail(server_url != NULL && device_key != NULL, NULL);
  g_return_val_if_fail(params != NULL, NULL);

  DatakiChatRequest *req = g_new0(DatakiChatRequest, 1);

  g_autofree char *base = g_strdup(server_url);
  gsize len = strlen(base);
  if (len > 0 && base[len - 1] == '/')
    base[len - 1] = '\0';
  req->url = g_strconcat(base, "/chat_handler.php", NULL);

  req->bearer    = g_strdup(device_key);
  req->content   = g_strdup(params->content);
  req->chat_id   = params->chat_id;
  req->model     = g_strdup(params->model);
  req->effort    = params->effort ? g_strdup(params->effort) : NULL;
  req->thinking  = params->thinking;
  req->websearch = params->websearch;
  req->on_event  = on_event;
  req->on_done   = on_done;
  req->user_data = user_data;
  req->sse       = dataki_sse_new(on_sse_payload, req);

  req->thread = g_thread_new("dataki-chat", chat_thread, req);
  return req;
}

void
dataki_chat_cancel(DatakiChatRequest *req)
{
  if (req != NULL)
    g_atomic_int_set(&req->cancel, 1);
}

void
dataki_chat_request_free(DatakiChatRequest *req)
{
  if (req == NULL)
    return;
  if (req->thread != NULL)
    g_thread_join(req->thread);
  g_clear_pointer(&req->sse, dataki_sse_free);
  g_free(req->url);
  g_free(req->bearer);
  g_free(req->content);
  g_free(req->model);
  g_free(req->effort);
  g_free(req);
}
