#pragma once

#include <glib.h>

G_BEGIN_DECLS

/*
 * Server-Sent-Events line parser (Architecture §9.4). Buffers incoming bytes
 * and emits the JSON payload of each complete `data:` line. Network chunks
 * split lines (and multi-byte UTF-8) arbitrarily, so only complete lines are
 * emitted and the remainder is kept. `[DONE]`, blank lines and SSE comments
 * (`:`) are ignored. GTK-free and unit-tested.
 */

typedef void (*DatakiSseCallback)(const char *data_json, gpointer user_data);

typedef struct _DatakiSse DatakiSse;

DatakiSse *dataki_sse_new(DatakiSseCallback callback, gpointer user_data);

void dataki_sse_feed(DatakiSse *self, const char *data, gsize len);

/* Emit any buffered trailing line that had no final newline. */
void dataki_sse_flush(DatakiSse *self);

void dataki_sse_free(DatakiSse *self);

G_END_DECLS
