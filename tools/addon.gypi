{
  'target_defaults': {
    'type': 'loadable_module',
    'product_extension': 'node',
    'include_dirs': [
      '../src',
      '../deps/uv/include',
      '../deps/v8/include'
    ],

    'conditions': [
      [ 'OS=="mac"', {
        'libraries': [ '-undefined dynamic_lookup' ],
      }]
    ]
  }
}
