#include "sse.h"

#include <string.h>

struct _DatakiSse
{
  GString          *buffer;
  DatakiSseCallback callback;
  gpointer          user_data;
};

DatakiSse *
dataki_sse_new(DatakiSseCallback callback, gpointer user_data)
{
  DatakiSse *self = g_new0(DatakiSse, 1);
  self->buffer    = g_string_new(NULL);
  self->callback  = callback;
  self->user_data = user_data;
  return self;
}

static void
handle_line(DatakiSse *self, const char *line_start, gsize line_len)
{
  /* Trim a trailing CR. */
  if (line_len > 0 && line_start[line_len - 1] == '\r')
    line_len--;

  if (line_len == 0)
    return; /* blank line (event separator) */

  g_autofree char *line = g_strndup(line_start, line_len);
  g_strstrip(line);

  if (line[0] == '\0' || line[0] == ':')
    return; /* empty after strip, or SSE comment */

  if (!g_str_has_prefix(line, "data:"))
    return; /* ignore event:/id:/retry: */

  const char *payload = line + 5; /* after "data:" */
  while (*payload == ' ')
    payload++;

  if (*payload == '\0' || g_strcmp0(payload, "[DONE]") == 0)
    return;

  if (self->callback)
    self->callback(payload, self->user_data);
}

void
dataki_sse_feed(DatakiSse *self, const char *data, gsize len)
{
  if (self == NULL || data == NULL || len == 0)
    return;

  g_string_append_len(self->buffer, data, len);

  gsize start = 0;
  for (gsize i = 0; i < self->buffer->len; i++)
    {
      if (self->buffer->str[i] == '\n')
        {
          handle_line(self, self->buffer->str + start, i - start);
          start = i + 1;
        }
    }

  if (start > 0)
    g_string_erase(self->buffer, 0, start);
}

void
dataki_sse_flush(DatakiSse *self)
{
  if (self == NULL || self->buffer->len == 0)
    return;
  handle_line(self, self->buffer->str, self->buffer->len);
  g_string_truncate(self->buffer, 0);
}

void
dataki_sse_free(DatakiSse *self)
{
  if (self == NULL)
    return;
  g_string_free(self->buffer, TRUE);
  g_free(self);
}
