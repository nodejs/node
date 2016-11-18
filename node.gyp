{
  'variables': {
    'v8_use_snapshot%': 'false',
    'node_use_dtrace%': 'false',
    'node_use_lttng%': 'false',
    'node_use_etw%': 'false',
    'node_use_perfctr%': 'false',
    'node_no_browser_globals%': 'false',
    'node_use_v8_platform%': 'true',
    'node_use_bundled_v8%': 'true',
    'node_shared%': 'false',
    'force_dynamic_crt%': 0,
    'node_module_version%': '',
    'node_shared_zlib%': 'false',
    'node_shared_http_parser%': 'false',
    'node_shared_cares%': 'false',
    'node_shared_libuv%': 'false',
    'node_use_openssl%': 'true',
    'node_shared_openssl%': 'false',
    'node_v8_options%': '',
    'node_enable_v8_vtunejit%': 'false',
    'node_core_target_name%': 'node',
    'library_files': [
      'lib/internal/bootstrap_node.js',
      'lib/_debug_agent.js',
      'lib/_debugger.js',
      'lib/assert.js',
      'lib/buffer.js',
      'lib/child_process.js',
      'lib/console.js',
      'lib/constants.js',
      'lib/crypto.js',
      'lib/cluster.js',
      'lib/dgram.js',
      'lib/dns.js',
      'lib/domain.js',
      'lib/events.js',
      'lib/fs.js',
      'lib/http.js',
      'lib/_http_agent.js',
      'lib/_http_client.js',
      'lib/_http_common.js',
      'lib/_http_incoming.js',
      'lib/_http_outgoing.js',
      'lib/_http_server.js',
      'lib/https.js',
      'lib/_linklist.js',
      'lib/module.js',
      'lib/net.js',
      'lib/os.js',
      'lib/path.js',
      'lib/process.js',
      'lib/punycode.js',
      'lib/querystring.js',
      'lib/readline.js',
      'lib/repl.js',
      'lib/stream.js',
      'lib/_stream_readable.js',
      'lib/_stream_writable.js',
      'lib/_stream_duplex.js',
      'lib/_stream_transform.js',
      'lib/_stream_passthrough.js',
      'lib/_stream_wrap.js',
      'lib/string_decoder.js',
      'lib/sys.js',
      'lib/timers.js',
      'lib/tls.js',
      'lib/_tls_common.js',
      'lib/_tls_legacy.js',
      'lib/_tls_wrap.js',
      'lib/tty.js',
      'lib/url.js',
      'lib/util.js',
      'lib/v8.js',
      'lib/vm.js',
      'lib/zlib.js',
      'lib/internal/buffer.js',
      'lib/internal/child_process.js',
      'lib/internal/cluster.js',
      'lib/internal/freelist.js',
      'lib/internal/fs.js',
      'lib/internal/linkedlist.js',
      'lib/internal/net.js',
      'lib/internal/module.js',
      'lib/internal/process/next_tick.js',
      'lib/internal/process/promises.js',
      'lib/internal/process/stdio.js',
      'lib/internal/process/warning.js',
      'lib/internal/process.js',
      'lib/internal/readline.js',
      'lib/internal/repl.js',
      'lib/internal/socket_list.js',
      'lib/internal/url.js',
      'lib/internal/util.js',
      'lib/internal/v8_prof_polyfill.js',
      'lib/internal/v8_prof_processor.js',
      'lib/internal/streams/lazy_transform.js',
      'lib/internal/streams/BufferList.js',
      'deps/v8/tools/splaytree.js',
      'deps/v8/tools/codemap.js',
      'deps/v8/tools/consarray.js',
      'deps/v8/tools/csvparser.js',
      'deps/v8/tools/profile.js',
      'deps/v8/tools/profile_view.js',
      'deps/v8/tools/logreader.js',
      'deps/v8/tools/tickprocessor.js',
      'deps/v8/tools/SourceMap.js',
      'deps/v8/tools/tickprocessor-driver.js',
    ],
    'conditions': [
      [ 'node_shared=="true"', {
        'node_target_type%': 'shared_library',
      }, {
        'node_target_type%': 'executable',
      }],
      [ 'OS=="win" and '
        'node_use_openssl=="true" and '
        'node_shared_openssl=="false"', {
        'use_openssl_def': 1,
      }, {
        'use_openssl_def': 0,
      }],
    ],
  },

  'targets': [
    {
      'target_name': '<(node_core_target_name)',
      'type': '<(node_target_type)',

      'dependencies': [
        'node_js2c#host',
      ],

      'include_dirs': [
        'src',
        'tools/msvs/genfiles',
        'deps/uv/src/ares',
        '<(SHARED_INTERMEDIATE_DIR)', # for node_natives.h
      ],

      'sources': [
        'src/debug-agent.cc',
        'src/async-wrap.cc',
        'src/env.cc',
        'src/fs_event_wrap.cc',
        'src/cares_wrap.cc',
        'src/connection_wrap.cc',
        'src/connect_wrap.cc',
        'src/handle_wrap.cc',
        'src/js_stream.cc',
        'src/node.cc',
        'src/node_buffer.cc',
        'src/node_config.cc',
        'src/node_constants.cc',
        'src/node_contextify.cc',
        'src/node_debug_options.cc',
        'src/node_file.cc',
        'src/node_http_parser.cc',
        'src/node_javascript.cc',
        'src/node_main.cc',
        'src/node_os.cc',
        'src/node_revert.cc',
        'src/node_url.cc',
        'src/node_util.cc',
        'src/node_v8.cc',
        'src/node_stat_watcher.cc',
        'src/node_watchdog.cc',
        'src/node_zlib.cc',
        'src/node_i18n.cc',
        'src/pipe_wrap.cc',
        'src/signal_wrap.cc',
        'src/spawn_sync.cc',
        'src/string_bytes.cc',
        'src/stream_base.cc',
        'src/stream_wrap.cc',
        'src/tcp_wrap.cc',
        'src/timer_wrap.cc',
        'src/tty_wrap.cc',
        'src/process_wrap.cc',
        'src/udp_wrap.cc',
        'src/uv.cc',
        # headers to make for a more pleasant IDE experience
        'src/async-wrap.h',
        'src/async-wrap-inl.h',
        'src/base-object.h',
        'src/base-object-inl.h',
        'src/connection_wrap.h',
        'src/connect_wrap.h',
        'src/debug-agent.h',
        'src/env.h',
        'src/env-inl.h',
        'src/handle_wrap.h',
        'src/js_stream.h',
        'src/node.h',
        'src/node_buffer.h',
        'src/node_constants.h',
        'src/node_debug_options.h',
        'src/node_file.h',
        'src/node_http_parser.h',
        'src/node_internals.h',
        'src/node_javascript.h',
        'src/node_mutex.h',
        'src/node_root_certs.h',
        'src/node_version.h',
        'src/node_watchdog.h',
        'src/node_wrap.h',
        'src/node_revert.h',
        'src/node_i18n.h',
        'src/pipe_wrap.h',
        'src/tty_wrap.h',
        'src/tcp_wrap.h',
        'src/udp_wrap.h',
        'src/req-wrap.h',
        'src/req-wrap-inl.h',
        'src/string_bytes.h',
        'src/stream_base.h',
        'src/stream_base-inl.h',
        'src/stream_wrap.h',
        'src/tree.h',
        'src/util.h',
        'src/util-inl.h',
        'src/util.cc',
        'src/string_search.cc',
        'deps/http_parser/http_parser.h',
        'deps/v8/include/v8.h',
        'deps/v8/include/v8-debug.h',
        '<(SHARED_INTERMEDIATE_DIR)/node_natives.h',
        # javascript files to make for an even more pleasant IDE experience
        '<@(library_files)',
        # node.gyp is added to the project by default.
        'common.gypi',
      ],

      'defines': [
        'NODE_ARCH="<(target_arch)"',
        'NODE_PLATFORM="<(OS)"',
        'NODE_WANT_INTERNALS=1',
        # Warn when using deprecated V8 APIs.
        'V8_DEPRECATION_WARNINGS=1',
      ],


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
        [ 'v8_inspector=="true"', {
          'defines': [
            'HAVE_INSPECTOR=1',
          ],
          'sources': [
            'src/inspector_agent.cc',
            'src/inspector_socket.cc',
            'src/inspector_socket_server.cc',
            'src/inspector_agent.h',
            'src/inspector_socket.h',
            'src/inspector_socket_server.h',
          ],
          'dependencies': [
            'deps/v8_inspector/src/inspector/inspector.gyp:standalone_inspector',
            'v8_inspector_compress_protocol_json#host',
          ],
          'include_dirs': [
            'deps/v8_inspector/include',
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
                            '<(PRODUCT_DIR)/obj.target/deps/openssl/'
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
                       '-O0' ]
        }],
        [ 'OS=="sunos"', {
          'ldflags': [ '-Wl,-M,/usr/lib/ld/map.noexstk' ],
        }],
      ],
    },
    {
      'target_name': 'mkssldef',
      'type': 'none',
      # TODO(bnoordhuis) Make all platforms export the same list of symbols.
      # Teach mkssldef.py to generate linker maps that UNIX linkers understand.
      'conditions': [
        [ 'use_openssl_def==1', {
          'variables': {
            'mkssldef_flags': [
              # Categories to export.
              '-CAES,BF,BIO,DES,DH,DSA,EC,ECDH,ECDSA,ENGINE,EVP,HMAC,MD4,MD5,'
              'NEXTPROTONEG,PSK,RC2,RC4,RSA,SHA,SHA0,SHA1,SHA256,SHA512,SOCK,'
              'STDIO,TLSEXT',
              # Defines.
              '-DWIN32',
              # Symbols to filter from the export list.
              '-X^DSO',
              '-X^_',
              '-X^private_',
              # Base generated DEF on zlib.def
              '-Bdeps/zlib/win32/zlib.def'
            ],
          },
          'conditions': [
            ['openssl_fips!=""', {
              'variables': { 'mkssldef_flags': ['-DOPENSSL_FIPS'] },
            }],
          ],
          'actions': [
            {
              'action_name': 'mkssldef',
              'inputs': [
                'deps/openssl/openssl/util/libeay.num',
                'deps/openssl/openssl/util/ssleay.num',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/openssl.def'],
              'action': [
                'python',
                'tools/mkssldef.py',
                '<@(mkssldef_flags)',
                '-o',
                '<@(_outputs)',
                '<@(_inputs)',
              ],
            },
          ],
        }],
      ],
    },
    # generate ETW header and resource files
    {
      'target_name': 'node_etw',
      'type': 'none',
      'conditions': [
        [ 'node_use_etw=="true"', {
          'actions': [
            {
              'action_name': 'node_etw',
              'inputs': [ 'src/res/node_etw_provider.man' ],
              'outputs': [
                'tools/msvs/genfiles/node_etw_provider.rc',
                'tools/msvs/genfiles/node_etw_provider.h',
                'tools/msvs/genfiles/node_etw_providerTEMP.BIN',
              ],
              'action': [ 'mc <@(_inputs) -h tools/msvs/genfiles -r tools/msvs/genfiles' ]
            }
          ]
        } ]
      ]
    },
    # generate perf counter header and resource files
    {
      'target_name': 'node_perfctr',
      'type': 'none',
      'conditions': [
        [ 'node_use_perfctr=="true"', {
          'actions': [
            {
              'action_name': 'node_perfctr_man',
              'inputs': [ 'src/res/node_perfctr_provider.man' ],
              'outputs': [
                'tools/msvs/genfiles/node_perfctr_provider.h',
                'tools/msvs/genfiles/node_perfctr_provider.rc',
                'tools/msvs/genfiles/MSG00001.BIN',
              ],
              'action': [ 'ctrpp <@(_inputs) '
                          '-o tools/msvs/genfiles/node_perfctr_provider.h '
                          '-rc tools/msvs/genfiles/node_perfctr_provider.rc'
              ]
            },
          ],
        } ]
      ]
    },
    {
      'target_name': 'v8_inspector_compress_protocol_json',
      'type': 'none',
      'toolsets': ['host'],
      'conditions': [
        [ 'v8_inspector=="true"', {
          'actions': [
            {
              'action_name': 'v8_inspector_compress_protocol_json',
              'process_outputs_as_sources': 1,
              'inputs': [
                'deps/v8_inspector/src/inspector/js_protocol.json',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/v8_inspector_protocol_json.h',
              ],
              'action': [
                'python',
                'tools/compress_json.py',
                '<@(_inputs)',
                '<@(_outputs)',
              ],
            },
          ],
        }],
      ],
    },
    {
      'target_name': 'node_js2c',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'node_js2c',
          'inputs': [
            '<@(library_files)',
            './config.gypi',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/node_natives.h',
          ],
          'conditions': [
            [ 'node_use_dtrace=="false" and node_use_etw=="false"', {
              'inputs': [ 'src/notrace_macros.py' ]
            }],
            ['node_use_lttng=="false"', {
              'inputs': [ 'src/nolttng_macros.py' ]
            }],
            [ 'node_use_perfctr=="false"', {
              'inputs': [ 'src/perfctr_macros.py' ]
            }]
          ],
          'action': [
            'python',
            'tools/js2c.py',
            '<@(_outputs)',
            '<@(_inputs)',
          ],
        },
      ],
    }, # end node_js2c
    {
      'target_name': 'node_dtrace_header',
      'type': 'none',
      'conditions': [
        [ 'node_use_dtrace=="true" and OS!="linux"', {
          'actions': [
            {
              'action_name': 'node_dtrace_header',
              'inputs': [ 'src/node_provider.d' ],
              'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/node_provider.h' ],
              'action': [ 'dtrace', '-h', '-xnolibs', '-s', '<@(_inputs)',
                '-o', '<@(_outputs)' ]
            }
          ]
        } ],
        [ 'node_use_dtrace=="true" and OS=="linux"', {
          'actions': [
            {
              'action_name': 'node_dtrace_header',
              'inputs': [ 'src/node_provider.d' ],
              'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/node_provider.h' ],
              'action': [ 'dtrace', '-h', '-s', '<@(_inputs)',
                '-o', '<@(_outputs)' ]
            }
          ]
        } ],
      ]
    },
    {
      'target_name': 'node_dtrace_provider',
      'type': 'none',
      'conditions': [
        [ 'node_use_dtrace=="true" and OS!="mac" and OS!="linux"', {
          'actions': [
            {
              'action_name': 'node_dtrace_provider_o',
              'inputs': [
                '<(OBJ_DIR)/node/src/node_dtrace.o',
              ],
              'outputs': [
                '<(OBJ_DIR)/node/src/node_dtrace_provider.o'
              ],
              'action': [ 'dtrace', '-G', '-xnolibs', '-s', 'src/node_provider.d',
                '<@(_inputs)', '-o', '<@(_outputs)' ]
            }
          ]
        }],
        [ 'node_use_dtrace=="true" and OS=="linux"', {
          'actions': [
            {
              'action_name': 'node_dtrace_provider_o',
              'inputs': [ 'src/node_provider.d' ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/node_dtrace_provider.o'
              ],
              'action': [
                'dtrace', '-C', '-G', '-s', '<@(_inputs)', '-o', '<@(_outputs)'
              ],
            }
          ],
        }],
      ]
    },
    {
      'target_name': 'node_dtrace_ustack',
      'type': 'none',
      'conditions': [
        [ 'node_use_dtrace=="true" and OS!="mac" and OS!="linux"', {
          'actions': [
            {
              'action_name': 'node_dtrace_ustack_constants',
              'inputs': [
                '<(V8_BASE)'
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/v8constants.h'
              ],
              'action': [
                'tools/genv8constants.py',
                '<@(_outputs)',
                '<@(_inputs)'
              ]
            },
            {
              'action_name': 'node_dtrace_ustack',
              'inputs': [
                'src/v8ustack.d',
                '<(SHARED_INTERMEDIATE_DIR)/v8constants.h'
              ],
              'outputs': [
                '<(OBJ_DIR)/node/src/node_dtrace_ustack.o'
              ],
              'conditions': [
                [ 'target_arch=="ia32"', {
                  'action': [
                    'dtrace', '-32', '-I<(SHARED_INTERMEDIATE_DIR)', '-Isrc',
                    '-C', '-G', '-s', 'src/v8ustack.d', '-o', '<@(_outputs)',
                  ]
                } ],
                [ 'target_arch=="x64"', {
                  'action': [
                    'dtrace', '-64', '-I<(SHARED_INTERMEDIATE_DIR)', '-Isrc',
                    '-C', '-G', '-s', 'src/v8ustack.d', '-o', '<@(_outputs)',
                  ]
                } ],
              ]
            },
          ]
        } ],
      ]
    },
    {
      'target_name': 'specialize_node_d',
      'type': 'none',
      'conditions': [
        [ 'node_use_dtrace=="true"', {
          'actions': [
            {
              'action_name': 'specialize_node_d',
              'inputs': [
                'src/node.d'
              ],
              'outputs': [
                '<(PRODUCT_DIR)/node.d',
              ],
              'action': [
                'tools/specialize_node_d.py',
                '<@(_outputs)',
                '<@(_inputs)',
                '<@(OS)',
                '<@(target_arch)',
              ],
            },
          ],
        } ],
      ]
    },
    {
      'target_name': 'cctest',
      'type': 'executable',
      'dependencies': [ 'deps/gtest/gtest.gyp:gtest' ],
      'include_dirs': [
        'src',
        'deps/v8/include',
        '<(SHARED_INTERMEDIATE_DIR)'
      ],
      'defines': [
        # gtest's ASSERT macros conflict with our own.
        'GTEST_DONT_DEFINE_ASSERT_EQ=1',
        'GTEST_DONT_DEFINE_ASSERT_GE=1',
        'GTEST_DONT_DEFINE_ASSERT_GT=1',
        'GTEST_DONT_DEFINE_ASSERT_LE=1',
        'GTEST_DONT_DEFINE_ASSERT_LT=1',
        'GTEST_DONT_DEFINE_ASSERT_NE=1',
        'NODE_WANT_INTERNALS=1',
      ],
      'sources': [
        'test/cctest/util.cc',
      ],

      'conditions': [
        ['v8_inspector=="true"', {
          'defines': [
            'HAVE_INSPECTOR=1',
          ],
          'dependencies': [
            'deps/zlib/zlib.gyp:zlib',
            'v8_inspector_compress_protocol_json#host'
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)'
          ],
          'sources': [
            'src/inspector_socket.cc',
            'src/inspector_socket_server.cc',
            'test/cctest/test_inspector_socket.cc',
            'test/cctest/test_inspector_socket_server.cc'
          ],
          'conditions': [
            [ 'node_shared_openssl=="false"', {
              'dependencies': [
                'deps/openssl/openssl.gyp:openssl'
              ]
            }],
            [ 'node_shared_http_parser=="false"', {
              'dependencies': [
                'deps/http_parser/http_parser.gyp:http_parser'
              ]
            }],
            [ 'node_shared_libuv=="false"', {
              'dependencies': [
                'deps/uv/uv.gyp:libuv'
              ]
            }]
          ]
        }],
        [ 'node_use_v8_platform=="true"', {
          'dependencies': [
            'deps/v8/src/v8.gyp:v8_libplatform',
          ],
        }],
        [ 'node_use_bundled_v8=="true"', {
          'dependencies': [
            'deps/v8/src/v8.gyp:v8',
            'deps/v8/src/v8.gyp:v8_libplatform'
          ],
        }],
      ]
    }
  ], # end targets

  'conditions': [
    ['OS=="aix"', {
      'targets': [
        {
          'target_name': 'node',
          'conditions': [
            ['node_shared=="true"', {
              'type': 'shared_library',
              'ldflags': ['--shared'],
              'product_extension': '<(shlib_suffix)',
            }, {
              'type': 'executable',
            }],
          ],
          'dependencies': ['<(node_core_target_name)', 'node_exp'],

          'include_dirs': [
            'src',
            'deps/v8/include',
          ],

          'sources': [
            'src/node_main.cc',
            '<@(library_files)',
            # node.gyp is added to the project by default.
            'common.gypi',
          ],

          'ldflags': ['-Wl,-bE:<(PRODUCT_DIR)/node.exp'],
        },
        {
          'target_name': 'node_exp',
          'type': 'none',
          'dependencies': [
            '<(node_core_target_name)',
          ],
          'actions': [
            {
              'action_name': 'expfile',
              'inputs': [
                '<(OBJ_DIR)'
              ],
              'outputs': [
                '<(PRODUCT_DIR)/node.exp'
              ],
              'action': [
                'sh', 'tools/create_expfile.sh',
                      '<@(_inputs)', '<@(_outputs)'
              ],
            }
          ]
        }
      ], # end targets
    }], # end aix section
  ], # end conditions block
}
