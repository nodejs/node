{
  'targets': [
    {
      'target_name': 'binding-export-default',
      'sources': [ 'binding-export-default.cc' ],
      'includes': ['../common.gypi'],
      'include_dirs': [ '<@(node_shared_libuv_include_dirs)' ],
    },
  ]
}
