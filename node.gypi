{
  'conditions': [
    [ 'node_shared=="false"', {
      'msvs_settings': {
        'VCManifestTool': {
          'EmbedManifest': 'true',
          'AdditionalManifestFiles': 'src/res/node.exe.extra.manifest'
        }
      },
    }, {
      'defines': [
        'NODE_SHARED_MODE',
      ],
      'conditions': [
        [ 'node_module_version!="" and OS!="win"', {
          'product_extension': '<(shlib_suffix)',
        }]
      ],
    }],
    [ 'node_enable_d8=="true"', {
      'dependencies': [ 'deps/v8/src/d8.gyp:d8' ],
    }],
    [ 'node_use_bundled_v8=="true"', {
      'dependencies': [
        'deps/v8/src/v8.gyp:v8',
        'deps/v8/src/v8.gyp:v8_libplatform'
      ],
    }],
    [ 'node_use_v8_platform=="true"', {
      'defines': [
        'NODE_USE_V8_PLATFORM=1',
      ],
    }, {
      'defines': [
        'NODE_USE_V8_PLATFORM=0',
      ],
    }],
    [ 'node_tag!=""', {
      'defines': [ 'NODE_TAG="<(node_tag)"' ],
    }],
    [ 'node_v8_options!=""', {
      'defines': [ 'NODE_V8_OPTIONS="<(node_v8_options)"'],
    }],
    # No node_main.cc for anything except executable
    [ 'node_target_type!="executable"', {
      'sources!': [
        'src/node_main.cc',
      ],
    }],
    [ 'node_release_urlbase!=""', {
      'defines': [
        'NODE_RELEASE_URLBASE="<(node_release_urlbase)"',
      ]
    }],
    [
      'debug_http2==1', {
      'defines': [ 'NODE_DEBUG_HTTP2=1' ]
    }],
    [ 'v8_enable_i18n_support==1', {
      'defines': [ 'NODE_HAVE_I18N_SUPPORT=1' ],
      'dependencies': [
        '<(icu_gyp_path):icui18n',
        '<(icu_gyp_path):icuuc',
      ],
      'conditions': [
        [ 'icu_small=="true"', {
          'defines': [ 'NODE_HAVE_SMALL_ICU=1' ],
      }]],
    }],
    [ 'node_use_bundled_v8=="true" and \
       node_enable_v8_vtunejit=="true" and (target_arch=="x64" or \
       target_arch=="ia32" or target_arch=="x32")', {
      'defines': [ 'NODE_ENABLE_VTUNE_PROFILING' ],
      'dependencies': [
        'deps/v8/src/third_party/vtune/v8vtune.gyp:v8_vtune'
      ],
    }],
    [ 'node_use_lttng=="true"', {
      'defines': [ 'HAVE_LTTNG=1' ],
      'include_dirs': [ '<(SHARED_INTERMEDIATE_DIR)' ],
      'libraries': [ '-llttng-ust' ],
      'sources': [
        'src/node_lttng.cc'
      ],
    } ],
    [ 'node_use_etw=="true" and node_target_type!="static_library"', {
      'defines': [ 'HAVE_ETW=1' ],
      'dependencies': [ 'node_etw' ],
      'sources': [
        'src/node_win32_etw_provider.h',
        'src/node_win32_etw_provider-inl.h',
        'src/node_win32_etw_provider.cc',
        'src/node_dtrace.cc',
        'tools/msvs/genfiles/node_etw_provider.h',
        'tools/msvs/genfiles/node_etw_provider.rc',
      ]
    } ],
    [ 'node_use_perfctr=="true" and node_target_type!="static_library"', {
      'defines': [ 'HAVE_PERFCTR=1' ],
      'dependencies': [ 'node_perfctr' ],
      'sources': [
        'src/node_win32_perfctr_provider.h',
        'src/node_win32_perfctr_provider.cc',
        'src/node_counters.cc',
        'src/node_counters.h',
        'tools/msvs/genfiles/node_perfctr_provider.rc',
      ]
    } ],
    [ 'node_no_browser_globals=="true"', {
      'defines': [ 'NODE_NO_BROWSER_GLOBALS' ],
    } ],
    [ 'node_use_bundled_v8=="true" and v8_postmortem_support=="true"', {
      'dependencies': [ 'deps/v8/src/v8.gyp:postmortem-metadata' ],
      'conditions': [
        # -force_load is not applicable for the static library
        [ 'node_target_type!="static_library"', {
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-force_load,<(V8_BASE)',
            ],
          },
        }],
      ],
    }],
    [ 'node_shared_zlib=="false"', {
      'dependencies': [ 'deps/zlib/zlib.gyp:zlib' ],
    }],

    [ 'node_shared_http_parser=="false"', {
      'dependencies': [ 'deps/http_parser/http_parser.gyp:http_parser' ],
    }],

    [ 'node_shared_cares=="false"', {
      'dependencies': [ 'deps/cares/cares.gyp:cares' ],
    }],

    [ 'node_shared_libuv=="false"', {
      'dependencies': [ 'deps/uv/uv.gyp:libuv' ],
    }],

    [ 'node_shared_nghttp2=="false"', {
      'dependencies': [ 'deps/nghttp2/nghttp2.gyp:nghttp2' ],
    }],

    [ 'OS=="mac"', {
      # linking Corefoundation is needed since certain OSX debugging tools
      # like Instruments require it for some features
      'libraries': [ '-framework CoreFoundation' ],
      'defines!': [
        'NODE_PLATFORM="mac"',
      ],
      'defines': [
        # we need to use node's preferred "darwin" rather than gyp's preferred "mac"
        'NODE_PLATFORM="darwin"',
      ],
    }],
    [ 'OS=="freebsd"', {
      'libraries': [
        '-lutil',
        '-lkvm',
      ],
    }],
    [ 'OS=="aix"', {
      'defines': [
        '_LINUX_SOURCE_COMPAT',
      ],
    }],
    [ 'OS=="solaris"', {
      'libraries': [
        '-lkstat',
        '-lumem',
      ],
      'defines!': [
        'NODE_PLATFORM="solaris"',
      ],
      'defines': [
        # we need to use node's preferred "sunos"
        # rather than gyp's preferred "solaris"
        'NODE_PLATFORM="sunos"',
      ],
    }],
    [ '(OS=="freebsd" or OS=="linux") and node_shared=="false" and coverage=="false"', {
      'ldflags': [ '-Wl,-z,noexecstack',
                   '-Wl,--whole-archive <(V8_BASE)',
                   '-Wl,--no-whole-archive' ]
    }],
    [ '(OS=="freebsd" or OS=="linux") and node_shared=="false" and coverage=="true"', {
      'ldflags': [ '-Wl,-z,noexecstack',
                   '-Wl,--whole-archive <(V8_BASE)',
                   '-Wl,--no-whole-archive',
                   '--coverage',
                   '-g',
                   '-O0' ],
       'cflags': [ '--coverage',
                   '-g',
                   '-O0' ],
       'cflags!': [ '-O3' ]
    }],
    [ 'OS=="mac" and node_shared=="false" and coverage=="true"', {
      'xcode_settings': {
        'OTHER_LDFLAGS': [
          '--coverage',
        ],
        'OTHER_CFLAGS+': [
          '--coverage',
          '-g',
          '-O0'
        ],
      }
    }],
    [ 'OS=="sunos"', {
      'ldflags': [ '-Wl,-M,/usr/lib/ld/map.noexstk' ],
    }],
  ],
}
