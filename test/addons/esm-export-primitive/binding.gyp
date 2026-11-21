{
  'targets': [
    {
      'target_name': 'binding-export-primitive',
      'sources': [ 'binding-export-primitive.cc' ],
      'includes': ['../common.gypi'],
      'include_dirs': [ '<@(node_shared_libuv_include_dirs)' ],
    },
  ]
}
