{
  'targets': [
    {
      'target_name': 'ffi_test_library',
      'sources': ['ffi_test_library.c'],
      'type': 'shared_library',
      'conditions': [
        ['OS in "aix os400"', {
          'product_extension': 'so',
          'ldflags': [ '-Wl,-G' ],
        }],
      ],
    }
  ]
}
