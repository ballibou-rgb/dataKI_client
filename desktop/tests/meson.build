glib_dep    = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')

config_test = executable('config-test',
  files('config_test.c', '../src/core/config.c'),
  dependencies: [glib_dep, gobject_dep],
  include_directories: include_directories('../src/core'))

test('config', config_test)
