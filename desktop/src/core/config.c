#include "config.h"

#define DATAKI_CONFIG_GROUP "general"

struct _DatakiConfig
{
  GObject    parent_instance;
  GKeyFile  *keyfile;
  char      *path;
  gboolean   dirty;
};

G_DEFINE_TYPE(DatakiConfig, dataki_config, G_TYPE_OBJECT)

static char *
config_file_path(void)
{
  /* g_get_user_config_dir() maps to $XDG_CONFIG_HOME on Linux and
   * %APPDATA% on Windows, which is exactly what we want. */
  return g_build_filename(g_get_user_config_dir(), "dataki", "client.ini", NULL);
}

static void
dataki_config_finalize(GObject *object)
{
  DatakiConfig *self = DATAKI_CONFIG(object);

  if (self->dirty)
    dataki_config_save(self);

  g_clear_pointer(&self->keyfile, g_key_file_free);
  g_clear_pointer(&self->path, g_free);

  G_OBJECT_CLASS(dataki_config_parent_class)->finalize(object);
}

static void
dataki_config_init(DatakiConfig *self)
{
  self->keyfile = g_key_file_new();
  self->path    = config_file_path();
  self->dirty   = FALSE;

  g_autoptr(GError) error = NULL;
  if (!g_key_file_load_from_file(self->keyfile, self->path,
                                 G_KEY_FILE_KEEP_COMMENTS, &error))
    {
      if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_warning("Could not read config %s: %s", self->path, error->message);
      /* Missing file is normal on first run. */
    }
}

static void
dataki_config_class_init(DatakiConfigClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = dataki_config_finalize;
}

DatakiConfig *
dataki_config_new(void)
{
  return g_object_new(DATAKI_TYPE_CONFIG, NULL);
}

char *
dataki_config_get_string(DatakiConfig *self, const char *key, const char *fallback)
{
  g_return_val_if_fail(DATAKI_IS_CONFIG(self), g_strdup(fallback));

  char *value = g_key_file_get_string(self->keyfile, DATAKI_CONFIG_GROUP, key, NULL);
  return value != NULL ? value : g_strdup(fallback);
}

void
dataki_config_set_string(DatakiConfig *self, const char *key, const char *value)
{
  g_return_if_fail(DATAKI_IS_CONFIG(self));

  g_key_file_set_string(self->keyfile, DATAKI_CONFIG_GROUP, key, value ? value : "");
  self->dirty = TRUE;
}

gint
dataki_config_get_int(DatakiConfig *self, const char *key, gint fallback)
{
  g_return_val_if_fail(DATAKI_IS_CONFIG(self), fallback);

  g_autoptr(GError) error = NULL;
  gint value = g_key_file_get_integer(self->keyfile, DATAKI_CONFIG_GROUP, key, &error);
  return error != NULL ? fallback : value;
}

void
dataki_config_set_int(DatakiConfig *self, const char *key, gint value)
{
  g_return_if_fail(DATAKI_IS_CONFIG(self));

  g_key_file_set_integer(self->keyfile, DATAKI_CONFIG_GROUP, key, value);
  self->dirty = TRUE;
}

gboolean
dataki_config_save(DatakiConfig *self)
{
  g_return_val_if_fail(DATAKI_IS_CONFIG(self), FALSE);

  g_autofree char *dir = g_path_get_dirname(self->path);
  if (g_mkdir_with_parents(dir, 0700) != 0)
    {
      g_warning("Could not create config directory %s", dir);
      return FALSE;
    }

  g_autoptr(GError) error = NULL;
  if (!g_key_file_save_to_file(self->keyfile, self->path, &error))
    {
      g_warning("Could not write config %s: %s", self->path, error->message);
      return FALSE;
    }

  self->dirty = FALSE;
  return TRUE;
}
