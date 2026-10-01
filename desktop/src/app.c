#include "app.h"

#include <glib/gi18n.h>

#include "config.h"
#include "core/config.h"
#include "core/tray.h"
#include "ui/main_window.h"
#include "ui/about_dialog.h"

struct _DatakiApp
{
  GtkApplication parent_instance;

  DatakiMainWindow *window;
  DatakiConfig     *config;
  DatakiTray       *tray;

  gboolean quitting;         /* a real quit is in progress               */
  gboolean tray_hint_shown;  /* "runs in the tray" notice already shown  */
};

G_DEFINE_TYPE(DatakiApp, dataki_app, GTK_TYPE_APPLICATION)

/* -------------------------------------------------------------------------- */
/* Tray callbacks                                                             */
/* -------------------------------------------------------------------------- */

static void
on_tray_show(gpointer user_data)
{
  dataki_app_show_window(DATAKI_APP(user_data));
}

static void
on_tray_quit(gpointer user_data)
{
  dataki_app_request_quit(DATAKI_APP(user_data));
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void
dataki_app_show_window(DatakiApp *self)
{
  g_return_if_fail(DATAKI_IS_APP(self));
  if (self->window == NULL)
    return;

  gtk_widget_show(GTK_WIDGET(self->window));
  gtk_window_present(GTK_WINDOW(self->window));
}

void
dataki_app_request_quit(DatakiApp *self)
{
  g_return_if_fail(DATAKI_IS_APP(self));

  self->quitting = TRUE;
  g_clear_pointer(&self->tray, dataki_tray_free);
  g_application_quit(G_APPLICATION(self));
}

gboolean
dataki_app_handle_window_close(DatakiApp *self)
{
  g_return_val_if_fail(DATAKI_IS_APP(self), FALSE);

  /* A real quit is running, or there is no tray to minimise into. */
  if (self->quitting || self->tray == NULL)
    return FALSE;

  gtk_widget_hide(GTK_WIDGET(self->window));

  if (!self->tray_hint_shown)
    {
      self->tray_hint_shown = TRUE;

      GNotification *n = g_notification_new("dataKI");
      g_notification_set_body(n, _("dataKI keeps running in the system tray."));
      g_application_send_notification(G_APPLICATION(self), "tray-hint", n);
      g_object_unref(n);
    }

  return TRUE;
}

/* -------------------------------------------------------------------------- */
/* GActions (☰ menu)                                                          */
/* -------------------------------------------------------------------------- */

static void
action_about(GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void)action;
  (void)param;
  DatakiApp *self = DATAKI_APP(user_data);
  dataki_about_dialog_show(self->window ? GTK_WINDOW(self->window) : NULL);
}

static void
action_quit(GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void)action;
  (void)param;
  dataki_app_request_quit(DATAKI_APP(user_data));
}

static const GActionEntry app_actions[] = {
  { "about", action_about, NULL, NULL, NULL, {0} },
  { "quit",  action_quit,  NULL, NULL, NULL, {0} },
};

/* -------------------------------------------------------------------------- */
/* GApplication vfuncs                                                         */
/* -------------------------------------------------------------------------- */

static void
dataki_app_startup(GApplication *application)
{
  G_APPLICATION_CLASS(dataki_app_parent_class)->startup(application);

  DatakiApp *self = DATAKI_APP(application);

  gtk_window_set_default_icon_name(DATAKI_APP_ID);

  /* Also load the bundled icon so it shows even when running uninstalled. */
  {
    g_autoptr(GError) icon_error = NULL;
    GdkPixbuf *icon = gdk_pixbuf_new_from_resource(
        "/ovh/datanet/dataki/client/icons/app-256.png", &icon_error);
    if (icon != NULL)
      {
        gtk_window_set_default_icon(icon);
        g_object_unref(icon);
      }
    else
      {
        g_warning("Could not load bundled app icon: %s", icon_error->message);
      }
  }

  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource(provider,
      "/ovh/datanet/dataki/client/css/style.css");
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
      GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);

  g_action_map_add_action_entries(G_ACTION_MAP(self),
                                  app_actions, G_N_ELEMENTS(app_actions),
                                  self);

  const char *quit_accels[] = { "<Primary>q", NULL };
  gtk_application_set_accels_for_action(GTK_APPLICATION(self),
                                        "app.quit", quit_accels);
}

static void
dataki_app_activate(GApplication *application)
{
  DatakiApp *self = DATAKI_APP(application);

  if (self->window == NULL)
    {
      self->window = dataki_main_window_new(GTK_APPLICATION(self));
      g_object_add_weak_pointer(G_OBJECT(self->window), (gpointer *)&self->window);

      self->tray = dataki_tray_new(DATAKI_APP_ID,
                                   on_tray_show, on_tray_quit, self);
      if (self->tray == NULL)
        g_message("No system tray available — closing the window will quit.");
    }

  dataki_app_show_window(self);
}

static void
dataki_app_shutdown(GApplication *application)
{
  DatakiApp *self = DATAKI_APP(application);

  g_clear_pointer(&self->tray, dataki_tray_free);
  g_clear_object(&self->config);

  G_APPLICATION_CLASS(dataki_app_parent_class)->shutdown(application);
}

/* -------------------------------------------------------------------------- */
/* Boilerplate                                                                */
/* -------------------------------------------------------------------------- */

static void
dataki_app_init(DatakiApp *self)
{
  self->config = dataki_config_new();
}

static void
dataki_app_class_init(DatakiAppClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS(klass);
  app_class->startup  = dataki_app_startup;
  app_class->activate = dataki_app_activate;
  app_class->shutdown = dataki_app_shutdown;
}

DatakiApp *
dataki_app_new(void)
{
  return g_object_new(DATAKI_TYPE_APP,
                      "application-id", DATAKI_APP_ID,
                      "flags", G_APPLICATION_DEFAULT_FLAGS,
                      NULL);
}
