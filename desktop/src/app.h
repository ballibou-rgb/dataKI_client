#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DATAKI_TYPE_APP (dataki_app_get_type())
G_DECLARE_FINAL_TYPE(DatakiApp, dataki_app, DATAKI, APP, GtkApplication)

DatakiApp *dataki_app_new(void);

/* Bring the main window to the foreground (used by the tray "Open" entry). */
void dataki_app_show_window(DatakiApp *self);

/* Perform a real quit (used by the ☰→Quit action and the tray "Quit" entry). */
void dataki_app_request_quit(DatakiApp *self);

/*
 * Called by the main window's delete-event handler.
 * Returns TRUE when the close was intercepted (window hidden to tray) and
 * FALSE when the window should be destroyed (no tray, or a real quit is in
 * progress).
 */
gboolean dataki_app_handle_window_close(DatakiApp *self);

G_END_DECLS
