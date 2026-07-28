{
  'variables': {
    'histogram_sources': [
      'src/hdr_histogram.c',
      'include/hdr/hdr_histogram.h',
    ]
  },
  'targets': [
    {
      'target_name': 'histogram',
      'type': 'static_library',
      'cflags': ['-fvisibility=hidden'],
      'xcode_settings': {
        'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',  # -fvisibility=hidden
      },
      'conditions': [
        [ 'clang==1', {
          'conditions': [
            [ 'OS=="win"', {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'AdditionalOptions': [
                    '-Wno-atomic-alignment',
                    '-Wno-incompatible-pointer-types',
                    '-Wno-unused-function'
                  ],
                },
              },
            }, {
              'cflags': [
                '-Wno-atomic-alignment',
                '-Wno-incompatible-pointer-types',
                '-Wno-unused-function'
              ],
            }],
          ],
        }],
      ],
      'include_dirs': ['src', 'include'],
      'direct_dependent_settings': {
        'include_dirs': [ 'src', 'include' ]
      },
      'sources': [
        '<@(histogram_sources)',
      ]
    }
  ]
}
