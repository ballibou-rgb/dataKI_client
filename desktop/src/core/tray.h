#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct _DatakiTray DatakiTray;

typedef void (*DatakiTrayCallback)(gpointer user_data);

/*
 * Create a system-tray icon with an "Open" and a "Quit" entry.
 *
 *   icon_name : themed icon name (Linux) / ignored on Windows, which loads the
 *               executable's embedded icon.
 *   on_show   : invoked when the user asks to show the window.
 *   on_quit   : invoked when the user asks to quit.
 *
 * Returns NULL when no tray backend is available (e.g. appindicator missing,
 * or a desktop without StatusNotifier support). Callers must handle NULL by
 * letting the window close quit the app.
 */
DatakiTray *dataki_tray_new(const char         *icon_name,
                            DatakiTrayCallback  on_show,
                            DatakiTrayCallback  on_quit,
                            gpointer            user_data);

/* NULL-safe. */
void dataki_tray_free(DatakiTray *self);

G_END_DECLS
