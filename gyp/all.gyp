{
  'target_defaults': {
    'configurations': {
      'Debug': {
        'defines': [ 'DEBUG', '_DEBUG' ]
      },
      'Release': {
        'defines': [ 'NDEBUG' ]
      }
    }
  },

  'variables': {
    'v8_use_snapshot': 'true',
    'target_arch': 'x64',
  },

  'targets': [
    {
      'target_name': 'node',
      'type': 'executable',

      'dependencies': [
        '../deps/http_parser/http_parser.gyp:http_parser',
        '../deps/v8/tools/gyp/v8.gyp:v8',
        '../deps/uv/build/all.gyp:uv',
        'node_js2c#host'
      ],
      'include_dirs': [ 'src' ],
      'sources': [
        '../src/cares_wrap.cc',
        '../src/handle_wrap.cc',
        '../src/node.cc',
        '../src/node_buffer.cc',
        '../src/node_constants.cc',
        '../src/node_crypto.cc',
        '../src/node_dtrace.cc',
        '../src/node_extensions.cc',
        '../src/node_file.cc',
        '../src/node_http_parser.cc',
        '../src/node_javascript.cc',
        '../src/node_main.cc',
        '../src/node_os.cc',
        '../src/node_script.cc',
        '../src/node_string.cc',
        '../src/pipe_wrap.cc',
        '../src/stdio_wrap.cc',
        '../src/stream_wrap.cc',
        '../src/tcp_wrap.cc',
        '../src/timer_wrap.cc',
      ],

      'conditions': [
        [ 'OS=="win"', {
          'defines': [
            'PTW32_STATIC_LIB'
          ],
          'libraries': [
            '-lws2_32',
            '-lwinmm',
            '../deps/pthread-win32/libpthreadGC2.a',
          ],
          'sources': [
            '../src/platform_win32.cc',
            '../src/node_stdio_win32.cc'
          ]
        },{ # POSIX
          'sources': [
            '../src/node_cares.cc',
            '../src/node_net.cc',
            '../src/node_signal_watcher.cc',
            '../src/node_stat_watcher.cc',
            '../src/node_io_watcher.cc',
            '../src/node_stdio.cc',
            '../src/node_child_process.cc',
            '../src/node_timer.cc'
          ]
        }],
        [ 'OS=="mac"', {
          'sources': [
            '../src/platform_darwin.cc', 
            '../src/platform_darwin_proctitle.cc'
          ]
        }]
      ]
    },

    {
      'target_name': 'node_js2c',
      'type': 'none',
      'toolsets': ['host'],
      'variables': {
        'library_files': [
          '../src/node.js',
          '../lib/_debugger.js',
          '../lib/_linklist.js',
          '../lib/assert.js',
          '../lib/buffer.js',
          '../lib/buffer_ieee754.js',
          '../lib/child_process_legacy.js',
          '../lib/child_process_uv.js',
          '../lib/console.js',
          '../lib/constants.js',
          '../lib/crypto.js',
          '../lib/dgram.js',
          '../lib/dns_legacy.js',
          '../lib/dns_uv.js',
          '../lib/events.js',
          '../lib/freelist.js',
          '../lib/fs.js',
          '../lib/http.js',
          '../lib/http2.js',
          '../lib/https.js',
          '../lib/https2.js',
          '../lib/module.js',
          '../lib/net_legacy.js',
          '../lib/net_uv.js',
          '../lib/os.js',
          '../lib/path.js',
          '../lib/punycode.js',
          '../lib/querystring.js',
          '../lib/readline.js',
          '../lib/repl.js',
          '../lib/stream.js',
          '../lib/string_decoder.js',
          '../lib/sys.js',
          '../lib/timers_legacy.js',
          '../lib/timers_uv.js',
          '../lib/tls.js',
          '../lib/tty.js',
          '../lib/tty_posix.js',
          '../lib/tty_win32.js',
          '../lib/url.js',
          '../lib/util.js',
          '../lib/vm.js',
        ],
      },
      'actions': [
        {
          'action_name': 'node_js2c',
          'inputs': [
            '../tools/js2c.py',
            '<@(library_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/node_natives.h',
          ],
          'action': [
            'python',
            '../tools/js2c.py',
            '<@(_outputs)',
            '<@(library_files)'
          ],
        },
      ],
    }, # end node_js2c
  ] # end targets
}

