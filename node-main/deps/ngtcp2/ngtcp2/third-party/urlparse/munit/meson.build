project('munit', 'c')

conf_data = configuration_data()
conf_data.set('version', '0.2.0')

add_project_arguments('-std=c99', language : 'c')

cc  = meson.get_compiler('c')

root_include = include_directories('.')

munit = library('munit',
    ['munit.c'],
    install: meson.is_subproject())

if meson.is_subproject()
  munit_dep = declare_dependency(
      include_directories : root_include,
      link_with : munit)
else
  # standalone install
  install_headers('munit.h')

  pkg = import('pkgconfig')
  pkg.generate(name: 'munit',
                description: 'µnit Testing Library for C',
                version: conf_data.get('version'),
                libraries: munit)

  # compile the demo project
  munit_example_src = files('example.c')
  munit_example = executable('munit_example', munit_example_src,
              include_directories: root_include,
              link_with: munit)

  test('munit example test', munit_example)
endif
