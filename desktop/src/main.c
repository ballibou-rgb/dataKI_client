#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <curl/curl.h>

#include "dataki-config.h"
#include "app.h"

int main(int argc, char **argv)
{
  setlocale(LC_ALL, "");

  /* Deterministic program name so the X11 WM_CLASS / Wayland app-id match the
   * installed .desktop file (StartupWMClass=dataki-client) and the taskbar
   * picks up the installed icon. */
  g_set_prgname("dataki-client");

  bindtextdomain(GETTEXT_PACKAGE, DATAKI_LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  textdomain(GETTEXT_PACKAGE);

  curl_global_init(CURL_GLOBAL_DEFAULT);

  DatakiApp *app = dataki_app_new();
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  curl_global_cleanup();
  return status;
}
