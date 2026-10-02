#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DATAKI_TYPE_MAIN_WINDOW (dataki_main_window_get_type())
G_DECLARE_FINAL_TYPE(DatakiMainWindow, dataki_main_window,
                     DATAKI, MAIN_WINDOW, GtkApplicationWindow)

DatakiMainWindow *dataki_main_window_new(GtkApplication *app);

/* Try to resume a stored session (keyring → bootstrap). Call once at startup. */
void dataki_main_window_try_autologin(DatakiMainWindow *self);

/* Revoke + forget the device key and return to the login page. */
void dataki_main_window_logout(DatakiMainWindow *self);

G_END_DECLS
