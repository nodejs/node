{
  'targets': [
    {
      'target_name': 'ffi_test_library',
      'sources': ['ffi_test_library.c'],
      'type': 'shared_library',
      'conditions': [
        [ 'OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              'ModuleDefinitionFile': 'ffi_test_library.def',
            },
          },
        }],
      ],
    }
  ]
}
