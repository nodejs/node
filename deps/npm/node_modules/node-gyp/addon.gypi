{
  'target_defaults': {
    'type': 'loadable_module',
    'product_extension': 'node',
    'product_prefix': '',
    'include_dirs': [
      '<(node_root_dir)/src',
      '<(node_root_dir)/deps/uv/include',
      '<(node_root_dir)/deps/v8/include'
    ],

    'conditions': [
      [ 'OS=="mac"', {
        'libraries': [ '-undefined dynamic_lookup' ],
      }],
      [ 'OS=="win"', {
        'libraries': [ '-l<(node_root_dir)/$(Configuration)/node.lib' ],
      }],
      [ 'OS=="freebsd" or OS=="openbsd" or OS=="solaris" or (OS=="linux" and target_arch!="ia32")', {
        'cflags': [ '-fPIC' ],
      }]
    ]
  }
}
