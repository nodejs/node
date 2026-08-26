{
  'variables': {
    'ncrypto_bssl_libdecrepit_missing%': 1,
    'ncrypto_sources': [
      'ncrypto.cc',
      'ncrypto.h',
    ],
    # OpenSSL may be configured with --api below 3.0. Keep 3.0 deprecation
    # checks where supported without exceeding its configured API level.
    'ncrypto_openssl3_api_compat':
      '(OPENSSL_CONFIGURED_API<30000?OPENSSL_CONFIGURED_API:30000)',
    'ncrypto_strict_defines': [
      'OPENSSL_API_COMPAT=<(ncrypto_openssl3_api_compat)',
      'OPENSSL_NO_DEPRECATED',
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
        }],
        ['node_shared_openssl=="false"', {
          'dependencies': [
            '../openssl/openssl.gyp:openssl'
          ]
        }],
      ]
    },
  ],
}
