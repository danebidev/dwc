project(
  'dwc',
  'cpp',
  'c',
  version: '0.1',
  default_options: [
    'cpp_std=c++20',
  ],
)

add_global_arguments('-DWLR_USE_UNSTABLE', language: 'c')

wayland_scanner = find_program('wayland-scanner')
pkg_config = find_program('pkg-config')

wayland_protos = dependency('wayland-protocols')

wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')

protocols = [
  'protocols/wlr-layer-shell-unstable-v1.xml',
  wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
  wl_protocol_dir / 'staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml',
  wl_protocol_dir / 'staging/ext-image-capture-source/ext-image-capture-source-v1.xml',
  wl_protocol_dir / 'staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml',
  wl_protocol_dir / 'unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml',
  wl_protocol_dir / 'unstable/xdg-output/xdg-output-unstable-v1.xml',
]

wl_protos_src = []

foreach xml : protocols
  wl_protos_src += custom_target(
    xml.underscorify() + '_c',
    input: xml,
    output: '@BASENAME@-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
  wl_protos_src += custom_target(
    xml.underscorify() + '_server_h',
    input: xml,
    output: '@BASENAME@-protocol.h',
    command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
  )
  wl_protos_src += custom_target(
    xml.underscorify() + '_client_h',
    input: xml,
    output: '@BASENAME@-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
endforeach

conf_data = configuration_data()
conf_data.set_quoted('PROGRAM_NAME', meson.project_name())
if get_option('buildtype').startswith('debug')
  conf_data.set('DEBUG', true)
endif

configure_file(
  output: 'build-config.h',
  configuration: conf_data,
)

include_dir = run_command(pkg_config, '--variable=includedir', 'wlroots-0.19').stdout().replace('\n', '')

include = include_directories('include', '.', include_dir, include_dir / 'wlroots-0.19')

libs = [
  wayland_protos,
  dependency('wayland-server'),
  dependency('wlroots-0.19'),
  dependency('xkbcommon'),
  dependency('libinput'),
]

sources = [
  'src/config/config.cpp',
  'src/config/commands.cpp',
  'src/wlr-wrapper.cpp',
  'src/util.cpp',
  'src/main.cpp',
  'src/workspace.cpp',
  'src/server.cpp',
  'src/output.cpp',
  'src/xdg-shell.cpp',
  'src/layer-shell.cpp',
  'src/input.cpp',
  'src/root.cpp',
  wl_protos_src,
]

executable(
  meson.project_name(),
  sources,
  include_directories: include,
  dependencies: libs,
  install: true,
  install_dir: get_option('bindir'),
)

install_data('dwc.desktop', install_dir: '//usr/share/wayland-sessions')
