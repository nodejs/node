{
  'target_defaults': {
    'type': 'loadable_module',
    'product_extension': 'node',
    'product_prefix': '',

    'conditions': [
      [ 'OS=="mac"', {
        'libraries': [ '-undefined dynamic_lookup' ]
      }],
      [ 'OS=="win"', {
        'include_dirs': [
          '../src',
          '../deps/uv/include',
          '../deps/v8/include'
        ],
        'libraries': [ '-l<(node_root_dir>/Debug/node.lib' ],
      }, {
        'include_dirs': ['../../../include/node']
      }]
    ]
  }
}
