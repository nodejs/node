{
  'includes': ['../test.gypi'],
  'targets': [{
    'target_name': 'testlib',
    'type': 'none',
    'dependencies': ['testlib/testlib.gyp:testlib'],
    'sources': ['proxy.cc'],
  }],
}
