{
  'variables': {
    'ncrypto_bssl_libdecrepit_missing%': 1,
    'ncrypto_sources': [
      'engine.cc',
      'ncrypto.cc',
      'ncrypto.h',
    ],
  },
  'targets': [
    {
      'target_name': 'ncrypto',
      'type': 'static_library',
      'include_dirs': ['.'],
      'defines': [
        'NCRYPTO_BSSL_LIBDECREPIT_MISSING=<(ncrypto_bssl_libdecrepit_missing)',
      ],
      'direct_dependent_settings': {
        'include_dirs': ['.'],
        'defines': [
          'NCRYPTO_BSSL_LIBDECREPIT_MISSING=<(ncrypto_bssl_libdecrepit_missing)',
        ],
      },
      'sources': [ '<@(ncrypto_sources)' ],
      'conditions': [
        ['node_shared_openssl=="false"', {
          'dependencies': [
            '../openssl/openssl.gyp:openssl'
          ]
        }],
      ]
    },
  ]
}
