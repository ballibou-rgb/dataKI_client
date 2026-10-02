#pragma once

#include <glib.h>

G_BEGIN_DECLS

/*
 * Chat streaming transport (Architecture §7.1/§7.2). POSTs to chat_handler.php
 * as multipart/form-data with the device-key Bearer and streams the SSE
 * response. Runs on a worker thread; callbacks fire on the GTK main thread.
 */

typedef struct _DatakiChatRequest DatakiChatRequest;

typedef struct
{
  const char *content;
  gint64      chat_id;   /* 0 = new chat        */
  const char *model;     /* model id            */
  const char *effort;    /* effort id, or NULL  */
  gboolean    thinking;
  gboolean    websearch;
} DatakiChatParams;

/* Each SSE event (raw JSON payload), on the main thread. */
typedef void (*DatakiChatEventCallback)(const char *event_json, gpointer user_data);
/* Stream finished, on the main thread. error=NULL on success/cancel. */
typedef void (*DatakiChatDoneCallback)(const char *error, gpointer user_data);

DatakiChatRequest *dataki_chat_send(const char              *server_url,
                                    const char              *device_key,
                                    const DatakiChatParams  *params,
                                    DatakiChatEventCallback  on_event,
                                    DatakiChatDoneCallback   on_done,
                                    gpointer                 user_data);

/* Request abort (safe to call from the main thread). */
void dataki_chat_cancel(DatakiChatRequest *req);

/* Free the request (joins the worker). Call from the on_done handler. */
void dataki_chat_request_free(DatakiChatRequest *req);

G_END_DECLS
