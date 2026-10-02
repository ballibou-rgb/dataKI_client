#include "about_dialog.h"

#include <glib/gi18n.h>

#include "dataki-config.h"

void
dataki_about_dialog_show(GtkWindow *parent)
{
  const char *authors[] = { "dataNet.ovh Ltd.", NULL };

  gtk_show_about_dialog(parent,
                        "program-name",   "dataKI",
                        "logo-icon-name", DATAKI_APP_ID,
                        "version",        DATAKI_VERSION,
                        "comments",       _("Native client for dataKI."),
                        "copyright",      "© dataNet.ovh Ltd.",
                        "website",        "https://ai.datanet.ovh",
                        "website-label",  _("Open in browser"),
                        "authors",        authors,
                        "license-type",   GTK_LICENSE_CUSTOM,
                        "license",        "© dataNet.ovh Ltd. All rights reserved.",
                        NULL);
}
