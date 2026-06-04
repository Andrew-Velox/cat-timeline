project('cat-timeline', 'c',
  version : '0.1.0',
  default_options : ['c_std=c99', 'warning_level=2'])

# Dependencies resolved via pkg-config.
gtk_dep   = dependency('gtk+-3.0')
cairo_dep = dependency('cairo')
m_dep     = meson.get_compiler('c').find_library('m', required : false)

# Optional: gtk-layer-shell anchors the widget to the desktop (bottom layer) on
# Wayland — it sits below apps like a normal desktop widget instead of floating
# on top. Without it, the app falls back to a borderless always-on-top window.
layershell_dep = dependency('gtk-layer-shell-0', required : false)

deps = [gtk_dep, cairo_dep, m_dep]
if layershell_dep.found()
  deps += layershell_dep
  add_project_arguments('-DHAVE_LAYER_SHELL', language : 'c')
endif

# cJSON ships as source inside assets/ and is compiled directly in.
inc = include_directories('assets')

sources = [
  'src/main.c',
  'src/window.c',
  'src/timeline.c',
  'src/cat.c',
  'src/tasks.c',
  'src/style.c',
  'src/input.c',
  'src/settings.c',
  'src/settings_window.c',
  'assets/cJSON.c',
]

executable('cat-timeline',
  sources,
  include_directories : inc,
  dependencies : deps,
  install : true)
