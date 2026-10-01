#include "tray.h"

#ifdef HAVE_APPINDICATOR

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libayatana-appindicator/app-indicator.h>

struct _DatakiTray
{
  AppIndicator      *indicator;
  GtkWidget         *menu;        /* owned by the indicator */
  DatakiTrayCallback on_show;
  DatakiTrayCallback on_quit;
  gpointer           user_data;
};

static void
on_show_activate(GtkMenuItem *item, gpointer user_data)
{
  (void)item;
  DatakiTray *self = user_data;
  if (self->on_show)
    self->on_show(self->user_data);
}

static void
on_quit_activate(GtkMenuItem *item, gpointer user_data)
{
  (void)item;
  DatakiTray *self = user_data;
  if (self->on_quit)
    self->on_quit(self->user_data);
}

DatakiTray *
dataki_tray_new(const char         *icon_name,
                DatakiTrayCallback  on_show,
                DatakiTrayCallback  on_quit,
                gpointer            user_data)
{
  DatakiTray *self = g_new0(DatakiTray, 1);
  self->on_show   = on_show;
  self->on_quit   = on_quit;
  self->user_data = user_data;

  self->indicator = app_indicator_new("ovh.datanet.dataki.client",
                                       icon_name ? icon_name : "dialog-information",
                                       APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
  app_indicator_set_status(self->indicator, APP_INDICATOR_STATUS_ACTIVE);
  app_indicator_set_title(self->indicator, "dataKI");

  self->menu = gtk_menu_new();

  GtkWidget *show_item = gtk_menu_item_new_with_label(_("Open"));
  g_signal_connect(show_item, "activate", G_CALLBACK(on_show_activate), self);
  gtk_menu_shell_append(GTK_MENU_SHELL(self->menu), show_item);

  gtk_menu_shell_append(GTK_MENU_SHELL(self->menu), gtk_separator_menu_item_new());

  GtkWidget *quit_item = gtk_menu_item_new_with_label(_("Quit"));
  g_signal_connect(quit_item, "activate", G_CALLBACK(on_quit_activate), self);
  gtk_menu_shell_append(GTK_MENU_SHELL(self->menu), quit_item);

  gtk_widget_show_all(self->menu);
  app_indicator_set_menu(self->indicator, GTK_MENU(self->menu));

  /* Middle-/secondary-click shows the window directly (left-click opens the
   * menu — an appindicator limitation). */
  app_indicator_set_secondary_activate_target(self->indicator, show_item);

  return self;
}

void
dataki_tray_free(DatakiTray *self)
{
  if (self == NULL)
    return;

  g_clear_object(&self->indicator);
  g_free(self);
}

#else /* !HAVE_APPINDICATOR */

/* No tray backend compiled in — report "unavailable" so the window close
 * quits the application. */

DatakiTray *
dataki_tray_new(const char         *icon_name,
                DatakiTrayCallback  on_show,
                DatakiTrayCallback  on_quit,
                gpointer            user_data)
{
  (void)icon_name;
  (void)on_show;
  (void)on_quit;
  (void)user_data;
  return NULL;
}

void
dataki_tray_free(DatakiTray *self)
{
  (void)self;
}

#endif /* HAVE_APPINDICATOR */
