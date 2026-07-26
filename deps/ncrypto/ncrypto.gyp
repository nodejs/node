{
  'variables': {
    'ncrypto_bssl_libdecrepit_missing%': 1,
    'ncrypto_sources': [
      'ncrypto.cc',
      'ncrypto.h',
    ],
    'ncrypto_engine_sources': [
      'engine.cc',
      'ncrypto.h',
    ],
    'ncrypto_strict_defines': [
      'OPENSSL_API_COMPAT=30000',
      'OPENSSL_NO_DEPRECATED',
    ],
    'ncrypto_engine_defines': [
      'OPENSSL_API_COMPAT=30000',
      'OPENSSL_SUPPRESS_DEPRECATED',
      'NCRYPTO_ENGINE_COMPAT=1',
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
        'conditions': [
          ['openssl_is_boringssl=="false"', {
            'defines': [ '<@(ncrypto_strict_defines)' ],
          }],
        ],
      },
      'sources': [ '<@(ncrypto_sources)' ],
      'conditions': [
        ['openssl_is_boringssl=="false"', {
          'defines': [ '<@(ncrypto_strict_defines)' ],
          'dependencies': [
            'ncrypto_engine',
          ],
        }],
        ['node_shared_openssl=="false"', {
          'dependencies': [
            '../openssl/openssl.gyp:openssl'
          ]
        }],
      ]
    },
  ],
  'conditions': [
    ['openssl_is_boringssl=="false"', {
      'targets': [
        {
          'target_name': 'ncrypto_engine',
          'type': 'static_library',
          'include_dirs': ['.'],
          'defines': [
            'NCRYPTO_BSSL_LIBDECREPIT_MISSING=<(ncrypto_bssl_libdecrepit_missing)',
            '<@(ncrypto_engine_defines)',
          ],
          'sources': [ '<@(ncrypto_engine_sources)' ],
          'conditions': [
            ['node_shared_openssl=="false"', {
              'dependencies': [
                '../openssl/openssl.gyp:openssl'
              ]
            }],
          ]
        },
      ],
    }],
  ],
}
