#include "loopback.h"

#include <gio/gio.h>
#include <string.h>

struct _DatakiLoopback
{
  GSocketService        *service;
  guint16                port;
  DatakiLoopbackCallback callback;
  gpointer               user_data;
  gboolean               fired;
};

static const char RESPONSE_OK[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-Type: text/html; charset=utf-8\r\n"
  "Connection: close\r\n"
  "\r\n"
  "<!doctype html><html lang=\"de\"><head><meta charset=\"utf-8\">"
  "<title>dataKI</title></head>"
  "<body style=\"font-family:sans-serif;text-align:center;margin-top:18vh\">"
  "<h2>dataKI</h2><p>Anmeldung abgeschlossen. Du kannst dieses Fenster "
  "schlie&szlig;en und zur App zur&uuml;ckkehren.</p></body></html>\r\n";

/* Parse "GET /callback?code=..&state=.. HTTP/1.1" → code/state (url-decoded). */
static void
parse_request(const char *request, char **out_code, char **out_state)
{
  *out_code  = NULL;
  *out_state = NULL;

  const char *q = strchr(request, '?');
  if (q == NULL)
    return;
  q++;

  const char *end = q;
  while (*end && *end != ' ' && *end != '\r' && *end != '\n')
    end++;

  g_autofree char *query = g_strndup(q, end - q);
  g_auto(GStrv) pairs = g_strsplit(query, "&", -1);
  for (int i = 0; pairs[i] != NULL; i++)
    {
      g_auto(GStrv) kv = g_strsplit(pairs[i], "=", 2);
      if (kv[0] == NULL || kv[1] == NULL)
        continue;
      g_autofree char *val = g_uri_unescape_string(kv[1], NULL);
      if (g_strcmp0(kv[0], "code") == 0)
        *out_code = g_strdup(val ? val : "");
      else if (g_strcmp0(kv[0], "state") == 0)
        *out_state = g_strdup(val ? val : "");
    }
}

static gboolean
on_incoming(GSocketService *service, GSocketConnection *connection,
            GObject *source_object, gpointer user_data)
{
  (void)service;
  (void)source_object;
  DatakiLoopback *self = user_data;

  GInputStream  *in  = g_io_stream_get_input_stream(G_IO_STREAM(connection));
  GOutputStream *out = g_io_stream_get_output_stream(G_IO_STREAM(connection));

  char buffer[4096];
  gssize n = g_input_stream_read(in, buffer, sizeof(buffer) - 1, NULL, NULL);
  if (n > 0)
    buffer[n] = '\0';
  else
    buffer[0] = '\0';

  g_output_stream_write_all(out, RESPONSE_OK, sizeof(RESPONSE_OK) - 1, NULL, NULL, NULL);
  g_output_stream_flush(out, NULL, NULL);
  g_io_stream_close(G_IO_STREAM(connection), NULL, NULL);

  if (!self->fired)
    {
      char *code = NULL, *state = NULL;
      parse_request(buffer, &code, &state);
      self->fired = TRUE;
      if (self->callback)
        self->callback(code, state, self->user_data);
      g_free(code);
      g_free(state);
    }

  return TRUE; /* handled */
}

DatakiLoopback *
dataki_loopback_new(DatakiLoopbackCallback callback, gpointer user_data, GError **error)
{
  DatakiLoopback *self = g_new0(DatakiLoopback, 1);
  self->callback  = callback;
  self->user_data = user_data;
  self->service   = g_socket_service_new();

  g_autoptr(GInetAddress) addr = g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4);
  g_autoptr(GSocketAddress) saddr = g_inet_socket_address_new(addr, 0);

  GSocketAddress *effective = NULL;
  if (!g_socket_listener_add_address(G_SOCKET_LISTENER(self->service),
                                     saddr, G_SOCKET_TYPE_STREAM,
                                     G_SOCKET_PROTOCOL_TCP, NULL,
                                     &effective, error))
    {
      dataki_loopback_free(self);
      return NULL;
    }

  self->port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(effective));
  g_object_unref(effective);

  g_signal_connect(self->service, "incoming", G_CALLBACK(on_incoming), self);
  g_socket_service_start(self->service);

  return self;
}

guint16
dataki_loopback_port(DatakiLoopback *self)
{
  return self != NULL ? self->port : 0;
}

void
dataki_loopback_free(DatakiLoopback *self)
{
  if (self == NULL)
    return;
  if (self->service != NULL)
    {
      g_socket_service_stop(self->service);
      g_object_unref(self->service);
    }
  g_free(self);
}
