{
  'targets': [
    {
      'target_name': 'binding',
      'defines': [ 'V8_DEPRECATION_WARNINGS=1' ],
      'sources': [ 'binding.c' ],
      'include_dirs': [ '<@(node_shared_libuv_include_dirs)' ],
    }
  ]
}
