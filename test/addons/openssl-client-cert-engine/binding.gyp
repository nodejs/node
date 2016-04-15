{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'binding.cc' ]
    },
    {
      'target_name': 'testengine',
      'type': 'shared_library',
      'sources': [ 'testengine.cc' ],
      'include_dirs': [ '../../../deps/openssl/openssl/include' ]
    }
  ]
}
