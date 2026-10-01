/* GTK-free unit test for the config store (round-trip through a temp dir). */

#include <glib.h>
#include <glib/gstdio.h>

#include "config.h"

static void
test_config_roundtrip(void)
{
  g_autofree char *tmp = g_dir_make_tmp("dataki-test-XXXXXX", NULL);
  g_assert_nonnull(tmp);
  g_setenv("XDG_CONFIG_HOME", tmp, TRUE);

  /* Write and persist. */
  {
    DatakiConfig *cfg = dataki_config_new();
    dataki_config_set_string(cfg, "last_model", "llama3");
    dataki_config_set_int(cfg, "window_width", 1200);
    g_assert_true(dataki_config_save(cfg));
    g_object_unref(cfg);
  }

  /* Read back from a fresh instance. */
  {
    DatakiConfig *cfg = dataki_config_new();

    g_autofree char *model = dataki_config_get_string(cfg, "last_model", "none");
    g_assert_cmpstr(model, ==, "llama3");

    g_assert_cmpint(dataki_config_get_int(cfg, "window_width", 0), ==, 1200);

    /* Missing keys fall back. */
    g_autofree char *missing = dataki_config_get_string(cfg, "nope", "fallback");
    g_assert_cmpstr(missing, ==, "fallback");
    g_assert_cmpint(dataki_config_get_int(cfg, "nope", 42), ==, 42);

    g_object_unref(cfg);
  }

  /* Cleanup. */
  g_autofree char *dir = g_build_filename(tmp, "dataki", NULL);
  g_autofree char *file = g_build_filename(dir, "client.ini", NULL);
  g_remove(file);
  g_rmdir(dir);
  g_rmdir(tmp);
}

int
main(int argc, char **argv)
{
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/config/roundtrip", test_config_roundtrip);
  return g_test_run();
}
