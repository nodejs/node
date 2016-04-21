{
  'targets': [
    {
      'target_name': 'testengine',
      'type': 'shared_library',
      'sources': [ 'testengine.cc' ],
      'include_dirs': [ '../../../deps/openssl/openssl/include' ]
    }
  ]
}
