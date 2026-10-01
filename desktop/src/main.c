#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "config.h"
#include "app.h"

int main(int argc, char **argv)
{
  setlocale(LC_ALL, "");
  bindtextdomain(GETTEXT_PACKAGE, DATAKI_LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  textdomain(GETTEXT_PACKAGE);

  DatakiApp *app = dataki_app_new();
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
