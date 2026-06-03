project('cat-timeline', 'c',
  version : '0.1.0',
  default_options : ['c_std=c99', 'warning_level=2'])

# Dependencies resolved via pkg-config.
gtk_dep   = dependency('gtk+-3.0')
cairo_dep = dependency('cairo')
m_dep     = meson.get_compiler('c').find_library('m', required : false)

# cJSON ships as source inside assets/ and is compiled directly in.
inc = include_directories('assets')

sources = [
  'src/main.c',
  'src/window.c',
  'src/timeline.c',
  'src/cat.c',
  'src/tasks.c',
  'src/tooltip.c',
  'src/input.c',
  'src/settings.c',
  'src/settings_window.c',
  'assets/cJSON.c',
]

executable('cat-timeline',
  sources,
  include_directories : inc,
  dependencies : [gtk_dep, cairo_dep, m_dep],
  install : true)
