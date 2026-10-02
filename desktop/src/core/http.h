#pragma once

#include <glib.h>

G_BEGIN_DECLS

/*
 * Minimal synchronous HTTP client (libcurl) for the login/bootstrap calls.
 * Call these on a worker thread, not the GTK main thread. Chat streaming gets
 * its own streaming path in M4.
 */

typedef struct
{
  long  status;   /* HTTP status code                 */
  char *body;     /* response body (NUL-terminated)   */
  gsize length;   /* body length in bytes             */
} DatakiHttpResponse;

void dataki_http_response_clear(DatakiHttpResponse *resp);

/* GET; bearer may be NULL. Returns TRUE on a completed request (any status). */
gboolean dataki_http_get(const char *url, const char *bearer,
                         DatakiHttpResponse *out, GError **error);

/* POST a JSON body; bearer may be NULL. */
gboolean dataki_http_post_json(const char *url, const char *bearer,
                               const char *json_body,
                               DatakiHttpResponse *out, GError **error);

G_END_DECLS
