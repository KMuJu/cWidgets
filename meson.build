project(
  'cWidgets',
  'c',
  version: '0.1.0',
  default_options: [
    'buildtype=debug', # -g, minimal optimization
    'warning_level=3',
    'optimization=3',
    'b_sanitize=address,undefined', # built-in sanitizers
  ],
)

cc = meson.get_compiler('c')
math_lib = cc.find_library('m', required: true)

# SCSS → CSS compile step
sass_prog = find_program('sass', required: true)

scss_file = files('scss/style.scss')
css_file = 'style.css'

scss_build = custom_target(
  'compile-scss',
  input: scss_file,
  output: css_file,
  command: [sass_prog, '@INPUT@', '@OUTPUT@'],
  build_by_default: true,
)
scss_dep = declare_dependency(sources: scss_build)

# Dependencies
libnm = dependency('libnm')
gtk = dependency('gtk4')
gtk4layershell = dependency('gtk4-layer-shell-0')
libwireplumber = dependency('wireplumber-0.5')
libcjson = dependency('libcjson')

deps = [
  libnm,
  gtk,
  gtk4layershell,
  libwireplumber,
  libcjson,
  scss_dep,
  math_lib,
]

src = [
  'src/main.c',
  'src/networking/networking.c',
  'src/bar/bar.c',
  'src/bar/wifi/wifi_icon.c',
  'src/bar/battery/battery.c',
  'src/bar/audio/audio.c',
  'src/bar/date_time/date_time.c',
  'src/bar/workspaces/workspaces.c',
  'src/util/util.c',
  'src/bluetooth/bt.c',
  'src/bluetooth/device.c',
  'src/bluetooth/adapter.c',
  'src/quicksettings/quicksettings.c',
  'src/quicksettings/header.c',
  'src/quicksettings/togglebutton.c',
  'src/quicksettings/wifi_page.c',
  'src/quicksettings/bluetooth_page.c',
  'src/quicksettings/page.c',
  'src/quicksettings/audio_slider.c',
]

inc_dirs = include_directories(
  'src',
  'src/bar',
  'src/networking',
  'src/util',
  'src/bluetooth',
  'src/quicksettings',
)

exe = executable(
  'cWidgets',
  sources: src,
  dependencies: deps,
  include_directories: inc_dirs,
  install: true,
)

run_target('run', command: [exe], depends: exe)
