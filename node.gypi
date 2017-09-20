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
    [ 'v8_enable_inspector==1', {
      'defines': [
        'HAVE_INSPECTOR=1',
      ],
      'sources': [
        'src/inspector_agent.cc',
        'src/inspector_io.cc',
        'src/inspector_socket.cc',
        'src/inspector_socket_server.cc',
        'src/inspector_agent.h',
        'src/inspector_io.h',
        'src/inspector_socket.h',
        'src/inspector_socket_server.h',
      ],
      'dependencies': [
        'v8_inspector_compress_protocol_json#host',
      ],
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)/include', # for inspector
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
    }, {
      'defines': [ 'HAVE_INSPECTOR=0' ]
    }],
    [ 'node_use_openssl=="true"', {
      'defines': [ 'HAVE_OPENSSL=1' ],
      'sources': [
        'src/node_crypto.cc',
        'src/node_crypto_bio.cc',
        'src/node_crypto_clienthello.cc',
        'src/node_crypto.h',
        'src/node_crypto_bio.h',
        'src/node_crypto_clienthello.h',
        'src/tls_wrap.cc',
        'src/tls_wrap.h'
      ],
      'conditions': [
        ['openssl_fips != ""', {
          'defines': [ 'NODE_FIPS_MODE' ],
        }],
        [ 'node_shared_openssl=="false"', {
          'dependencies': [
            './deps/openssl/openssl.gyp:openssl',

            # For tests
            './deps/openssl/openssl.gyp:openssl-cli',
          ],
          # Do not let unused OpenSSL symbols to slip away
          'conditions': [
            # -force_load or --whole-archive are not applicable for
            # the static library
            [ 'node_target_type!="static_library"', {
              'xcode_settings': {
                'OTHER_LDFLAGS': [
                  '-Wl,-force_load,<(PRODUCT_DIR)/<(OPENSSL_PRODUCT)',
                ],
              },
              'conditions': [
                ['OS in "linux freebsd" and node_shared=="false"', {
                  'ldflags': [
                    '-Wl,--whole-archive,'
                        '<(OBJ_DIR)/deps/openssl/'
                        '<(OPENSSL_PRODUCT)',
                    '-Wl,--no-whole-archive',
                  ],
                }],
                # openssl.def is based on zlib.def, zlib symbols
                # are always exported.
                ['use_openssl_def==1', {
                  'sources': ['<(SHARED_INTERMEDIATE_DIR)/openssl.def'],
                }],
                ['OS=="win" and use_openssl_def==0', {
                  'sources': ['deps/zlib/win32/zlib.def'],
                }],
              ],
            }],
          ],
        }]]
    }, {
      'defines': [ 'HAVE_OPENSSL=0' ]
    }],
    [ 'node_use_dtrace=="true"', {
      'defines': [ 'HAVE_DTRACE=1' ],
      'dependencies': [
        'node_dtrace_header',
        'specialize_node_d',
      ],
      'include_dirs': [ '<(SHARED_INTERMEDIATE_DIR)' ],

      #
      # DTrace is supported on linux, solaris, mac, and bsd.  There are
      # three object files associated with DTrace support, but they're
      # not all used all the time:
      #
      #   node_dtrace.o           all configurations
      #   node_dtrace_ustack.o    not supported on mac and linux
      #   node_dtrace_provider.o  All except OS X.  "dtrace -G" is not
      #                           used on OS X.
      #
      # Note that node_dtrace_provider.cc and node_dtrace_ustack.cc do not
      # actually exist.  They're listed here to trick GYP into linking the
      # corresponding object files into the final "node" executable.  These
      # object files are generated by "dtrace -G" using custom actions
      # below, and the GYP-generated Makefiles will properly build them when
      # needed.
      #
      'sources': [ 'src/node_dtrace.cc' ],
      'conditions': [
        [ 'OS=="linux"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/node_dtrace_provider.o'
          ],
        }],
        [ 'OS!="mac" and OS!="linux"', {
          'sources': [
            'src/node_dtrace_ustack.cc',
            'src/node_dtrace_provider.cc',
          ]
        }
      ] ]
    } ],
    [ 'node_use_lttng=="true"', {
      'defines': [ 'HAVE_LTTNG=1' ],
      'include_dirs': [ '<(SHARED_INTERMEDIATE_DIR)' ],
      'libraries': [ '-llttng-ust' ],
      'sources': [
        'src/node_lttng.cc'
      ],
    } ],
    [ 'node_use_etw=="true"', {
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
    [ 'node_use_perfctr=="true"', {
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

    [ 'OS=="win"', {
      'sources': [
        'src/backtrace_win32.cc',
        'src/res/node.rc',
      ],
      'defines!': [
        'NODE_PLATFORM="win"',
      ],
      'defines': [
        'FD_SETSIZE=1024',
        # we need to use node's preferred "win32" rather than gyp's preferred "win"
        'NODE_PLATFORM="win32"',
        '_UNICODE=1',
      ],
      'libraries': [ '-lpsapi.lib' ]
    }, { # POSIX
      'defines': [ '__POSIX__' ],
      'sources': [ 'src/backtrace_posix.cc' ],
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
    [ 'OS=="sunos"', {
      'ldflags': [ '-Wl,-M,/usr/lib/ld/map.noexstk' ],
    }],
  ],
}
