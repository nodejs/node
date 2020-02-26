{
  'targets': [
    {
      'target_name': 'binding',
      'includes': ['../common.gypi'],
      'variables': {
        # Skip this building on IBM i.
        'aix_variant_name': '<!(uname -s)',
      },
      'conditions': [
        ['node_use_openssl=="true" and '
         '"<(aix_variant_name)"!="OS400"', {
          'sources': ['binding.cc'],
          'include_dirs': ['../../../deps/openssl/openssl/include'],
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
