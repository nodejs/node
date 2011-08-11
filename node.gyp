{
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      # TODO: hoist these out and put them somewhere common, because
      #       RuntimeLibrary MUST MATCH across the entire project
      'Debug': {
        'defines': [ 'DEBUG', '_DEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 1, # static debug
            'Optimization': 0, # /Od, no optimization
          },
        },
      },
      'Release': {
        'defines': [ 'NDEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 0, # static release
            'Optimization': 3, # /Ox, full optimization
            'FavorSizeOrSpeed': 1, # /Ot, favour speed over size
            'InlineFunctionExpansion': 2, # /Ob2, inline anything eligible
            'WholeProgramOptimization': 'true', # /GL, whole program optimization, needed for LTCG
          },
          'VCLinkerTool': {
            'LinkTimeCodeGeneration': 1, # link-time code generation
          },
        },
      }
    },
    'msvs_settings': {
      'VCCLCompilerTool': {
        'StringPooling': 'true', # pool string literals
        'DebugInformationFormat': 3, # Generate a PDB
        'AdditionalOptions': [
          '/MP', # compile across multiple CPUs, VC2008 setting
        ],
        'MultiProcessorCompilation': 'true', # compile across multiple CPUs, VC2010 setting
      },
      'VCLibrarianTool': {
      },
      'VCLinkerTool': {
        'GenerateDebugInformation': 'true',
      },
    },
    'conditions': [
      ['OS == "win"', {
        'defines': [
          'WIN32',
          # we don't really want VC++ warning us about
          # how dangerous C functions are...
          '_CRT_SECURE_NO_DEPRECATE',
          # ... or that C implementations shouldn't use
          # POSIX names
          '_CRT_NONSTDC_NO_DEPRECATE',
        ],
      }]
    ],
  },

  'variables': {
    'v8_use_snapshot': 'true',
    'target_arch': 'ia32',
    'node_use_dtrace': 'false',
    'node_use_openssl': 'true'
  },

  'targets': [
    {
      'target_name': 'node',
      'type': 'executable',

      'dependencies': [
        'deps/http_parser/http_parser.gyp:http_parser',
        'deps/v8/tools/gyp/v8.gyp:v8',
        'deps/uv/all.gyp:uv',
        'node_js2c#host',
      ],

      'include_dirs': [
        'src',
        'deps/uv/src/ares',
        '<(SHARED_INTERMEDIATE_DIR)' # for node_natives.h
      ],

      'sources': [
        'src/cares_wrap.cc',
        'src/handle_wrap.cc',
        'src/node.cc',
        'src/node_buffer.cc',
        'src/node_constants.cc',
        'src/node_dtrace.cc',
        'src/node_extensions.cc',
        'src/node_file.cc',
        'src/node_http_parser.cc',
        'src/node_javascript.cc',
        'src/node_main.cc',
        'src/node_os.cc',
        'src/node_script.cc',
        'src/node_string.cc',
        'src/pipe_wrap.cc',
        'src/stdio_wrap.cc',
        'src/stream_wrap.cc',
        'src/tcp_wrap.cc',
        'src/timer_wrap.cc',
        'src/process_wrap.cc',
      ],

      'defines': [
        'ARCH="<(target_arch)"',
        'PLATFORM="<(OS)"',
        '_LARGEFILE_SOURCE',
        '_FILE_OFFSET_BITS=64',
      ],

      'conditions': [
        [ 'node_use_openssl=="true"', {
          'libraries': [ '-lssl', '-lcrypto' ],
          'defines': [ 'HAVE_OPENSSL=1' ],
          'sources': [ 'src/node_crypto.cc' ],
        }, {
          'defines': [ 'HAVE_OPENSSL=0' ]
        }],

        [ 'OS=="win"', {
          'dependencies': [
            'deps/uv/deps/pthread-win32/build/all.gyp:pthread-win32',
          ],
          # openssl is not built using gyp, and needs to be 
          # built separately and placed outside the hierarchy.
          # the dependencies aren't set up yet to put it in 
          # place, so I'm going to force it off indiscrimately
          # for the time being. Because the above condition has
          # already kicked in, it's not enough simply to turn
          # 'node_use_openssl' off; I need to undo its effects
          'node_use_openssl': 'false',
          'defines!': [ 'HAVE_OPENSSL=1' ],
          'defines': [ 'HAVE_OPENSSL=0' ],
          'libraries!': [ '-lssl', '-lcrypto' ],
          'sources!': [ 'src/node_crypto.cc' ],
          'sources': [
            'src/platform_win32.cc',
            'src/node_stdio_win32.cc',
            # file operations depend on eio to link. uv contains eio in unix builds, but not win32. So we need to compile it here instead.
            'deps/uv/src/eio/eio.c',
          ],
          'defines': [
            'PTW32_STATIC_LIB',
            'FD_SETSIZE=1024',
            # we need to use node's preferred "win32" rather than gyp's preferred "win"
            'PLATFORM="win32"',
          ],
          'libraries': [
            '-lws2_32.lib',
            '-lwinmm.lib',
          ],
          'msvs_settings': {
            'VCCLCompilerTool': {
              'WarningLevel': '3',
            },
          },
        },{ # POSIX
          'defines': [ '__POSIX__' ],
          'sources': [
            'src/node_cares.cc',
            'src/node_net.cc',
            'src/node_signal_watcher.cc',
            'src/node_stat_watcher.cc',
            'src/node_io_watcher.cc',
            'src/node_stdio.cc',
            'src/node_child_process.cc',
            'src/node_timer.cc'
          ]
        }],
        [ 'OS=="mac"', {
          'sources': [ 'src/platform_darwin.cc' ],
          'libraries': [ '-framework Carbon' ],
        }]
      ]
    },

    {
      'target_name': 'node_js2c',
      'type': 'none',
      'toolsets': ['host'],
      'variables': {
        'library_files': [
          'src/node.js',
          'lib/_debugger.js',
          'lib/_linklist.js',
          'lib/assert.js',
          'lib/buffer.js',
          'lib/buffer_ieee754.js',
          'lib/child_process_legacy.js',
          'lib/child_process_uv.js',
          'lib/console.js',
          'lib/constants.js',
          'lib/crypto.js',
          'lib/dgram.js',
          'lib/dns_legacy.js',
          'lib/dns_uv.js',
          'lib/events.js',
          'lib/freelist.js',
          'lib/fs.js',
          'lib/http.js',
          'lib/http2.js',
          'lib/https.js',
          'lib/https2.js',
          'lib/module.js',
          'lib/net_legacy.js',
          'lib/net_uv.js',
          'lib/os.js',
          'lib/path.js',
          'lib/punycode.js',
          'lib/querystring.js',
          'lib/readline.js',
          'lib/repl.js',
          'lib/stream.js',
          'lib/string_decoder.js',
          'lib/sys.js',
          'lib/timers_legacy.js',
          'lib/timers_uv.js',
          'lib/tls.js',
          'lib/tty.js',
          'lib/tty_posix.js',
          'lib/tty_win32.js',
          'lib/url.js',
          'lib/util.js',
          'lib/vm.js',
        ],
      },

      'actions': [

        {
          'action_name': 'node_js2c',

          'inputs': [
            './tools/js2c.py',
            '<@(library_files)',
          ],

          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/node_natives.h',
          ],

          # FIXME can the following conditions be shorted by just setting
          # macros.py into some variable which then gets included in the
          # action?

          'conditions': [
            [ 'node_use_dtrace=="true"', {
              'action': [
                'python',
                'tools/js2c.py',
                '<@(_outputs)',
                '<@(library_files)'
              ],
            }, { # No Dtrace
              'action': [
                'python',
                'tools/js2c.py',
                '<@(_outputs)',
                '<@(library_files)',
                'src/macros.py'
              ],
            }]
          ],
          'msvs_cygwin_shell': 0,
        },
      ],
    }, # end node_js2c
  ] # end targets
}

