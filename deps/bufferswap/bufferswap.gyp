{
  'variables': {
    'target_arch%': '',
  },
  'targets': [
    {
      'target_name': 'bufferswap',
      'type': 'static_library',
      'include_dirs': [ 'include', 'lib' ],
      'direct_dependent_settings': {
        'include_dirs': [ 'include' ],
        'defines': [ 'BUFFERSWAP_STATIC_DEFINE' ],
      },
      'sources': [ 'include/libbufferswap.h', 'lib/lib.c' ],
      'defines': [ 'BUFFERSWAP_STATIC_DEFINE' ],
      'conditions': [
        [ 'target_arch in "x64" and OS!="win"', {
          'cflags': [ '-mavx512vbmi' ],
          'xcode_settings': {
            'OTHER_CFLAGS': [ '-mavx512vbmi' ]
          },
        }],
        ['target_arch in "x64" and OS=="win"', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': [
                '/arch:AVX512'
              ],
            },
          },
        }],
      ],
    },
  ]
}
