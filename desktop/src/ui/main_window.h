#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DATAKI_TYPE_MAIN_WINDOW (dataki_main_window_get_type())
G_DECLARE_FINAL_TYPE(DatakiMainWindow, dataki_main_window,
                     DATAKI, MAIN_WINDOW, GtkApplicationWindow)

DatakiMainWindow *dataki_main_window_new(GtkApplication *app);

G_END_DECLS
