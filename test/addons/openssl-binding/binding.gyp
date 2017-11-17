{
  'includes': ['../../../config.gypi'],
  'variables': {
    'node_target_type%': '',
  },
  'targets': [
    {
      'target_name': 'binding',
      'conditions': [
        ['node_use_openssl=="true"', {
          'sources': ['binding.cc'],
          'include_dirs': ['../../../deps/openssl/openssl/include'],
          'conditions': [
            ['OS=="win" and node_target_type=="static_library"', {
	      'libraries': [
                '../../../../$(Configuration)/lib/<(OPENSSL_PRODUCT)'
              ],
            }],
          ],
        }]
      ]
    },
  ]
}
