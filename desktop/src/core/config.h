#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/*
 * Persistent client configuration (GKeyFile under the platform config dir:
 * $XDG_CONFIG_HOME/dataki/client.ini on Linux, %APPDATA%\dataki\client.ini on
 * Windows). No chat content is ever stored here — see docs/architecture.md §11.
 *
 * This is GTK-free core code.
 */

#define DATAKI_TYPE_CONFIG (dataki_config_get_type())
G_DECLARE_FINAL_TYPE(DatakiConfig, dataki_config, DATAKI, CONFIG, GObject)

DatakiConfig *dataki_config_new(void);

char *dataki_config_get_string(DatakiConfig *self, const char *key, const char *fallback);
void  dataki_config_set_string(DatakiConfig *self, const char *key, const char *value);

gint  dataki_config_get_int(DatakiConfig *self, const char *key, gint fallback);
void  dataki_config_set_int(DatakiConfig *self, const char *key, gint value);

/* Flush pending changes to disk. Returns TRUE on success. */
gboolean dataki_config_save(DatakiConfig *self);

G_END_DECLS
