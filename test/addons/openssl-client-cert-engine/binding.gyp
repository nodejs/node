{
  'targets': [
    {
      'target_name': 'testengine',
      'type': 'shared_library',
      'product_extension': 'engine',
      'sources': [ 'testengine.cc' ],
      'include_dirs': [ '../../../deps/openssl/openssl/include' ]
    }
  ]
}
