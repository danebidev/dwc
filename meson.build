project(
  'dwc',
  'cpp',
  'c',
  default_options: [
    'cpp_std=c++20',
  ],
)

add_global_arguments('-DWLR_USE_UNSTABLE', language: 'c')

wayland_scanner = find_program('wayland-scanner')
pkg_config = find_program('pkg-config')

protocol_sources = []
protocol_code = []

protocols_wl_stable = ['xdg-shell']

wl_protocols_dir = run_command(pkg_config, '--variable=pkgdatadir', 'wayland-protocols').stdout().replace('\n', '')
foreach protocol : protocols_wl_stable
  path = wl_protocols_dir / 'stable' / protocol / '@0@.xml'.format(protocol)

  out_h = '@0@-protocol.h'.format(protocol)
  header = custom_target(
    out_h,
    output: out_h,
    input: path,
    command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
  )
  protocol_sources += [header]

  out_c = '@0@-protocol.c'.format(protocol)
  code = custom_target(
    out_c,
    output: out_c,
    input: path,
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
  protocol_code += [code]
endforeach

protocols_wl_unstable = ['xdg-output']

foreach protocol : protocols_wl_unstable
  path = wl_protocols_dir / 'unstable' / protocol / '@0@-unstable-v1.xml'.format(protocol)

  out_h = '@0@-unstable-v1-protocol.h'.format(protocol)
  header = custom_target(
    out_h,
    output: out_h,
    input: path,
    command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
  )
  protocol_sources += [header]

  out_c = '@0@-unstable-v1-protocol.c'.format(protocol)
  code = custom_target(
    out_c,
    output: out_c,
    input: path,
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
  protocol_code += [code]
endforeach

protocols_wlr_unstable = [
  'wlr-layer-shell-unstable-v1',
]

foreach protocol : protocols_wlr_unstable
  path = 'protocols' / '@0@.xml'.format(protocol)

  out_h = '@0@-protocol.h'.format(protocol)
  header = custom_target(
    out_h,
    output: out_h,
    input: path,
    command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
  )
  protocol_sources += [header]

  out_c = '@0@-protocol.c'.format(protocol)
  code = custom_target(
    out_c,
    output: out_c,
    input: path,
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
  protocol_code += [code]
endforeach

conf_data = configuration_data()
conf_data.set_quoted('PROGRAM_NAME', meson.project_name())

configure_file(
  output: 'build-config.h',
  configuration: conf_data,
)

include_dir = run_command(pkg_config, '--variable=includedir', 'wlroots-0.19').stdout().replace('\n', '')

include = include_directories('include', '.', include_dir, include_dir / 'wlroots-0.19')

libs = [
  dependency('wayland-server'),
  dependency('wayland-protocols'),
  dependency('wlroots-0.19'),
]

sources = ['src/main.cpp', 'src/server.cpp', protocol_sources, protocol_code]

executable(
  meson.project_name(),
  sources,
  include_directories: include,
  dependencies: libs,
)
