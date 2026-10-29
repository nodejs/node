{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'binding.cc' ],
      'includes': ['../common.gypi'],
      'include_dirs': [ '<@(node_shared_libuv_include_dirs)' ],
    }
  ]
}
