{
  'targets': [
    {
      'target_name': 'binding',
      'includes': ['../common.gypi'],
      'conditions': [
        ['node_use_openssl=="true"', {
          'conditions': [
            ['OS in "aix os400"', {
              'variables': {
                # Used to differentiate `AIX` and `OS400`(IBM i).
                'aix_variant_name': '<!(uname -s)',
              },
              'conditions': [
                [ '"<(aix_variant_name)"!="OS400"', { # Not `OS400`(IBM i)
                  'sources': ['binding.cc'],
                  'include_dirs': ['../../../deps/openssl/openssl/include'],
                }],
              ],
            }, {
              'sources': ['binding.cc'],
              'include_dirs': ['../../../deps/openssl/openssl/include'],
            }],
          ],
        }],
        ['OS=="mac"', {
          'xcode_settings': {
            'OTHER_CFLAGS+': [
              '-Wno-deprecated-declarations',
            ],
          },
        }, {
          'cflags': ['-Wno-deprecated-declarations'],
        }],
      ],
    },
  ]
}
