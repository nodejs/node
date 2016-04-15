{
  'targets': [
    {
      'target_name': 'testengine',
      'type': 'none',
      'conditions': [
        ['OS=="mac" and '
         'node_use_openssl=="true" and '
         'node_shared=="false" and '
         'node_shared_openssl=="false"', {
          'type': 'shared_library',
          'sources': [ 'testengine.cc' ],
          'product_extension': 'engine',
          'include_dirs': ['../../../deps/openssl/openssl/include'],
          'link_settings': {
            'libraries': [
              '../../../../out/<(PRODUCT_DIR)/<(OPENSSL_PRODUCT)'
            ]
          },
        }]
      ]
    }
  ]
}
