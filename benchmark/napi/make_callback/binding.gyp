{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'binding.cc' ],
      'include_dirs': [ '<@(node_shared_libuv_include_dirs)' ],
    }
  ]
}
