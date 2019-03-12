{
  'includes': ['../test.gypi'],
  'targets': [{
    'target_name': 'proxy',
    'type': 'static_library',
    'sources': ['void.cc'],
    'dependencies': ['testlib'],
    'export_dependent_settings': ['testlib'],
  }, {
    'target_name': 'testlib',
    'type': 'static_library',
    'sources': ['main.cc'],
  }],
}
