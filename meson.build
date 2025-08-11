project(
  'cWidgets',
  'c',
  version: '0.1.0',
  default_options: [
    'buildtype=debug',                    # -g, minimal optimization
    'warning_level=3',
    'b_sanitize=address,undefined',       # built-in sanitizers
  ]
)

# Dependencies
libnm = dependency('libnm')
gtk = dependency('gtk4')
gtk4layershell = dependency('gtk4-layer-shell-0')
libpipewire = dependency('libpipewire-0.3')
libwireplumber = dependency('wireplumber-0.5')
libcjson = dependency('libcjson')

deps = [
  libnm,
  gtk,
  gtk4layershell,
  libpipewire,
  libwireplumber,
  libcjson,
]

src = [
  'src/main.c',
  'src/networking/networking.c',
  'src/bar/bar.c',
  'src/bar/wifi/wifi.c',
  'src/bar/battery/battery.c',
  'src/bar/audio/audio.c',
  'src/bar/date_time/date_time.c',
  'src/bar/workspaces/workspaces.c',
  'src/util/util.c',
]

inc_dirs = include_directories(
  'src',
  'src/bar',
  'src/networking',
  'src/util',
)

exe = executable(
  'cWidgets',
  sources: src,
  dependencies: deps,
  include_directories: inc_dirs,
  install: true,
)

run_target('run', command: [exe], depends: exe)
