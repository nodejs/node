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
      }],
      ['_type=="static_library"', {
        # set to `1` to *disable* the -T thin archive 'ld' flag.
        # older linkers don't support this flag.
        'standalone_static_library': '<(standalone_static_library)'
      }],
    ],

    'conditions': [
      [ 'OS=="mac"', {
        'defines': [ '_DARWIN_USE_64_BIT_INODE=1' ],
        'libraries': [ '-undefined dynamic_lookup' ],
        'xcode_settings': {
          'DYLIB_INSTALL_NAME_BASE': '@rpath'
        },
      }],
      [ 'OS=="win"', {
        'libraries': [
          '-lkernel32.lib',
          '-luser32.lib',
          '-lgdi32.lib',
          '-lwinspool.lib',
          '-lcomdlg32.lib',
          '-ladvapi32.lib',
          '-lshell32.lib',
          '-lole32.lib',
          '-loleaut32.lib',
          '-luuid.lib',
          '-lodbc32.lib',
          '-lDelayImp.lib',
          '-l"<(node_root_dir)/$(ConfigurationName)/node.lib"'
        ],
        # warning C4251: 'node::ObjectWrap::handle_' : class 'v8::Persistent<T>'
        # needs to have dll-interface to be used by clients of class 'node::ObjectWrap'
        'msvs_disabled_warnings': [ 4251 ],
      }, {
        # OS!="win"
        'defines': [ '_LARGEFILE_SOURCE', '_FILE_OFFSET_BITS=64' ],
      }],
      [ 'OS=="freebsd" or OS=="openbsd" or OS=="solaris" or (OS=="linux" and target_arch!="ia32")', {
        'cflags': [ '-fPIC' ],
      }]
    ]
  }
}
