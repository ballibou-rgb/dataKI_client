project('dataki-client', 'c',
  version: '0.1.0',
  meson_version: '>= 0.59.0',
  license: 'Proprietary',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'buildtype=debugoptimized',
  ])

gnome = import('gnome')
i18n  = import('i18n')

app_id          = 'ovh.datanet.dataki.client'
gettext_package = 'dataki-client'

cc          = meson.get_compiler('c')
host_system = host_machine.system()

# ---------------------------------------------------------------------------
# Dependencies
# M1 deliberately links against GTK3 only (plus the platform tray backend).
# curl / json-glib / md4c / libsecret are added from M2 onwards (see
# docs/milestones.md). The secret store is a stub in M1.
# ---------------------------------------------------------------------------
gtk_dep = dependency('gtk+-3.0', version: '>= 3.22')
deps    = [gtk_dep]

tray_src = []
if host_system == 'windows'
  tray_src += files('src/core/tray_win.c')
  deps += cc.find_library('shell32')
  deps += cc.find_library('user32')
  deps += cc.find_library('ole32')
else
  appindicator = dependency('ayatana-appindicator3-0.1', required: false)
  if appindicator.found()
    deps += appindicator
    add_project_arguments('-DHAVE_APPINDICATOR=1', language: 'c')
  else
    message('ayatana-appindicator3 not found — tray disabled, window close will quit.')
  endif
  tray_src += files('src/core/tray_linux.c')
endif

# ---------------------------------------------------------------------------
# Generated config.h
# ---------------------------------------------------------------------------
conf = configuration_data()
conf.set_quoted('DATAKI_VERSION',    meson.project_version())
conf.set_quoted('DATAKI_APP_ID',     app_id)
conf.set_quoted('GETTEXT_PACKAGE',   gettext_package)
conf.set_quoted('DATAKI_LOCALEDIR',  join_paths(get_option('prefix'), get_option('localedir')))
conf.set_quoted('DATAKI_SERVER_URL', 'https://ai.datanet.ovh')
configure_file(output: 'config.h', configuration: conf)
config_inc = [include_directories('.'), include_directories('src')]

# ---------------------------------------------------------------------------
# Resources (CSS, UI)
# ---------------------------------------------------------------------------
resources = gnome.compile_resources('dataki-resources',
  'data/dataki.gresource.xml',
  source_dir: 'data',
  c_name: 'dataki')

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
sources = files(
  'src/main.c',
  'src/app.c',
  'src/core/config.c',
  'src/core/secret.c',
  'src/ui/main_window.c',
  'src/ui/about_dialog.c',
) + tray_src + resources

# Windows version/icon resource
if host_system == 'windows'
  version_parts = meson.project_version().split('.')
  winconf = configuration_data()
  winconf.set('VERSION',   meson.project_version())
  winconf.set('VER_MAJOR', version_parts[0])
  winconf.set('VER_MINOR', version_parts[1])
  winconf.set('VER_PATCH', version_parts[2])
  version_rc = configure_file(
    input:         'packaging/windows/version.rc.in',
    output:        'version.rc',
    configuration: winconf)

  windows = import('windows')
  sources += windows.compile_resources(version_rc,
    include_directories: include_directories('packaging/windows'))
endif

executable('dataki-client',
  sources,
  dependencies: deps,
  include_directories: config_inc,
  win_subsystem: 'windows',
  install: true)

# ---------------------------------------------------------------------------
# Data files (Linux)
# ---------------------------------------------------------------------------
if host_system != 'windows'
  desktop_file = i18n.merge_file(
    input:       'data/ovh.datanet.dataki.client.desktop.in',
    output:      'ovh.datanet.dataki.client.desktop',
    type:        'desktop',
    po_dir:      'po',
    install:     true,
    install_dir: join_paths(get_option('datadir'), 'applications'))

  foreach size : ['16', '24', '32', '48', '64', '128', '256', '512']
    install_data(
      'data/icons/hicolor' / (size + 'x' + size) / 'apps/ovh.datanet.dataki.client.png',
      install_dir: get_option('datadir') / 'icons' / 'hicolor' / (size + 'x' + size) / 'apps')
  endforeach

  install_data('data/ovh.datanet.dataki.client.metainfo.xml',
    install_dir: join_paths(get_option('datadir'), 'metainfo'))
endif

subdir('po')

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
subdir('tests')
