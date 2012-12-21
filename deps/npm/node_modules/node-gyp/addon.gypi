{
  'target_defaults': {
    'type': 'loadable_module',
    'product_prefix': '',
    'include_dirs': [
      '<(node_root_dir)/src',
      '<(node_root_dir)/deps/uv/include',
      '<(node_root_dir)/deps/v8/include'
    ],

    'target_conditions': [
      ['_type=="loadable_module"', {
        'product_extension': 'node',
        'defines': [ 'BUILDING_NODE_EXTENSION' ],
      }]
    ],

    'conditions': [
      [ 'OS=="mac"', {
        'libraries': [ '-undefined dynamic_lookup' ],
        'xcode_settings': {
          'DYLIB_INSTALL_NAME_BASE': '@rpath'
        },
      }],
      [ 'OS=="win"', {
        'libraries': [ '-l<(node_root_dir)/$(Configuration)/node.lib' ],
        # warning C4251: 'node::ObjectWrap::handle_' : class 'v8::Persistent<T>'
        # needs to have dll-interface to be used by clients of class 'node::ObjectWrap'
        'msvs_disabled_warnings': [ 4251 ],
      }],
      [ 'OS=="freebsd" or OS=="openbsd" or OS=="solaris" or (OS=="linux" and target_arch!="ia32")', {
        'cflags': [ '-fPIC' ],
      }]
    ]
  }
}
