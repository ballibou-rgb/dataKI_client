#pragma once

#include <glib.h>

G_BEGIN_DECLS

/* Simple async JSON GET against the backend (worker thread → main callback). */

typedef void (*DatakiApiCallback)(gboolean    ok,
                                  long        status,
                                  const char *json,
                                  gpointer    user_data);

void dataki_api_get_async(const char        *server_url,
                          const char        *device_key,
                          const char        *path,
                          DatakiApiCallback  callback,
                          gpointer           user_data);

G_END_DECLS
