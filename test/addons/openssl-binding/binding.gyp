{
  'targets': [
    {
      'target_name': 'binding',
      'includes': ['../common.gypi'],
      'conditions': [
        ['node_use_openssl=="true"', {
          'sources': ['binding.cc'],
          'include_dirs': ['../../../deps/openssl/openssl/include'],
        }],
      ],
    },
  ]
}
