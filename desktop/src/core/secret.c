#include "secret.h"

/*
 * M1 stub implementation.
 *
 * Login/key management is not part of M1, so there is nothing to store yet.
 * These functions return "no backend available" so callers added in M3 fail
 * loudly rather than silently losing a key. The real libsecret / Windows
 * Credential Manager backends (secret_linux.c / secret_win.c) replace this
 * file in the login milestone.
 */

#define DATAKI_SECRET_ERROR (dataki_secret_error_quark())

static GQuark
dataki_secret_error_quark(void)
{
  return g_quark_from_static_string("dataki-secret-error");
}

gboolean
dataki_secret_store(const char *server, const char *account,
                    const char *device_key, GError **error)
{
  (void)server;
  (void)account;
  (void)device_key;
  g_set_error_literal(error, DATAKI_SECRET_ERROR, 0,
                      "Secret store not implemented yet (arrives in M3).");
  return FALSE;
}

char *
dataki_secret_lookup(const char *server, const char *account, GError **error)
{
  (void)server;
  (void)account;
  (void)error; /* no key, no error: first-run behaviour */
  return NULL;
}

gboolean
dataki_secret_clear(const char *server, const char *account, GError **error)
{
  (void)server;
  (void)account;
  (void)error;
  return TRUE;
}

gboolean
dataki_secret_is_available(void)
{
  return FALSE;
}
