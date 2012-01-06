{
  'target_defaults': {
    'type': 'loadable_module',
    'product_extension': 'node',
    'product_prefix': '',
    'include_dirs': [
      '../src',
      '../deps/uv/include',
      '../deps/v8/include'
    ],

    'conditions': [
      [ 'OS=="mac"', {
        'libraries': [ '-undefined dynamic_lookup' ],
      }],
      [ 'OS=="win"', {
        'libraries': [ '-l<(node_root_dir)/$(Configuration)/node.lib' ],
      }]
    ]
  }
}
