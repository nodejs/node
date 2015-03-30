{
  'targets': [{
    'target_name': 'test',
    'type': 'executable',
    'dependencies': [
      'testlib/testlib.gyp:proxy',
      'proxy/proxy.gyp:testlib',
    ],
  }],
}
