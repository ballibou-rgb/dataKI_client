#pragma once

#include <glib.h>

G_BEGIN_DECLS

/*
 * Desktop browser hand-off login (Architecture §5).
 *
 * dataki_login_start():
 *   1. opens a loopback listener,
 *   2. opens the system browser on /api/v1/client/login/start.php,
 *   3. captures the one-time code, verifies state,
 *   4. exchanges it for a device key, stores it in the keyring,
 *   5. fetches bootstrap,
 * then calls `callback` on the GTK main thread.
 */

typedef struct _DatakiLogin DatakiLogin;

/* ok=TRUE → bootstrap_json holds the bootstrap body (not owned by callee).
 * ok=FALSE → error holds a message. */
typedef void (*DatakiLoginDoneCallback)(gboolean    ok,
                                        const char *error,
                                        const char *bootstrap_json,
                                        gpointer    user_data);

DatakiLogin *dataki_login_start(const char              *server_url,
                                DatakiLoginDoneCallback  callback,
                                gpointer                 user_data,
                                GError                 **error);

void dataki_login_cancel(DatakiLogin *self);

/* Look up a stored device key for the server (keyring). NULL if none. */
char *dataki_login_stored_key(const char *server_url);

/* Remove the stored device key for the server. */
void dataki_login_forget_key(const char *server_url);

/* Async bootstrap with an existing key (startup auto-login). */
typedef void (*DatakiBootstrapCallback)(gboolean    ok,
                                        long        status,
                                        const char *bootstrap_json,
                                        gpointer    user_data);

void dataki_login_bootstrap_async(const char              *server_url,
                                  const char              *device_key,
                                  DatakiBootstrapCallback  callback,
                                  gpointer                 user_data);

/* Revoke the device key server-side, then forget it locally (logout). */
void dataki_login_logout_async(const char *server_url, const char *device_key);

G_END_DECLS
