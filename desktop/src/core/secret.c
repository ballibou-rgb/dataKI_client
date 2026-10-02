#include "secret.h"

#define DATAKI_SECRET_ERROR (dataki_secret_error_quark())

static GQuark
dataki_secret_error_quark(void)
{
  return g_quark_from_static_string("dataki-secret-error");
}

#ifdef HAVE_LIBSECRET

#include <libsecret/secret.h>

/* Secret Service schema: ovh.datanet.dataki, keyed by server + account. */
static const SecretSchema *
dataki_schema(void)
{
  static const SecretSchema schema = {
    "ovh.datanet.dataki", SECRET_SCHEMA_NONE,
    {
      { "server",  SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "account", SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 },
    }
  };
  return &schema;
}

gboolean
dataki_secret_store(const char *server, const char *account,
                    const char *device_key, GError **error)
{
  return secret_password_store_sync(
      dataki_schema(), SECRET_COLLECTION_DEFAULT,
      "dataKI device key", device_key, NULL, error,
      "server", server ? server : "",
      "account", account ? account : "default",
      NULL);
}

char *
dataki_secret_lookup(const char *server, const char *account, GError **error)
{
  return secret_password_lookup_sync(
      dataki_schema(), NULL, error,
      "server", server ? server : "",
      "account", account ? account : "default",
      NULL);
}

gboolean
dataki_secret_clear(const char *server, const char *account, GError **error)
{
  return secret_password_clear_sync(
      dataki_schema(), NULL, error,
      "server", server ? server : "",
      "account", account ? account : "default",
      NULL);
}

gboolean
dataki_secret_is_available(void)
{
  return TRUE;
}

#else /* !HAVE_LIBSECRET */

/* No secret backend compiled in (e.g. Windows Credential Manager not yet
 * wired up). Keys do not persist. */

gboolean
dataki_secret_store(const char *server, const char *account,
                    const char *device_key, GError **error)
{
  (void)server; (void)account; (void)device_key;
  g_set_error_literal(error, DATAKI_SECRET_ERROR, 0,
                      "No secret backend available on this platform.");
  return FALSE;
}

char *
dataki_secret_lookup(const char *server, const char *account, GError **error)
{
  (void)server; (void)account; (void)error;
  return NULL;
}

gboolean
dataki_secret_clear(const char *server, const char *account, GError **error)
{
  (void)server; (void)account; (void)error;
  return TRUE;
}

gboolean
dataki_secret_is_available(void)
{
  return FALSE;
}

#endif /* HAVE_LIBSECRET */
