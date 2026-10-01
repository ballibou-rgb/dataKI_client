#pragma once

#include <glib.h>

G_BEGIN_DECLS

/*
 * SecretStore — platform abstraction for the device key (see
 * docs/architecture.md §ADR-4, §11). Stores only the device key, never a
 * password.
 *
 *   Linux   : libsecret (Secret Service), schema "ovh.datanet.dataki"
 *   Windows : Windows Credential Manager, target "dataKI:ai.datanet.ovh"
 *
 * M1 ships a stub (login does not exist yet). The real backends land with the
 * login milestone (M3). The signatures below are the stable contract that the
 * platform implementations will fulfil.
 */

/* Store (or replace) the device key for the given server. */
gboolean dataki_secret_store(const char *server, const char *account,
                             const char *device_key, GError **error);

/* Look up the device key for the given server. Returns NULL if none is stored
 * (then *error is left unset). Free the result with g_free(). */
char *dataki_secret_lookup(const char *server, const char *account, GError **error);

/* Remove the stored device key for the given server. */
gboolean dataki_secret_clear(const char *server, const char *account, GError **error);

/* TRUE when a real secret backend is compiled in. */
gboolean dataki_secret_is_available(void);

G_END_DECLS
