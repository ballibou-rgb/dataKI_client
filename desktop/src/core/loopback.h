#pragma once

#include <glib.h>

G_BEGIN_DECLS

/*
 * One-shot loopback HTTP listener for the browser login hand-off.
 * Binds 127.0.0.1 on an ephemeral port, waits for GET /callback?code=…&state=…,
 * replies with a small "you can close this window" page, and fires the callback
 * once (on the GTK main thread).
 */

typedef struct _DatakiLoopback DatakiLoopback;

/* code/state may be NULL if the request was malformed. */
typedef void (*DatakiLoopbackCallback)(const char *code, const char *state,
                                       gpointer user_data);

DatakiLoopback *dataki_loopback_new(DatakiLoopbackCallback callback,
                                    gpointer user_data, GError **error);

guint16 dataki_loopback_port(DatakiLoopback *self);

void dataki_loopback_free(DatakiLoopback *self);

G_END_DECLS
