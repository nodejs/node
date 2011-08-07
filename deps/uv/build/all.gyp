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
  'targets': [
    {
      'target_name': 'uv',
      'type': 'static_library',
      'include_dirs': [ '../include' ],
      'direct_dependent_settings': {
        'include_dirs': [ '../include' ],
      },

      'defines': [
        'HAVE_CONFIG_H'
      ],
      'sources': [
        '../src/uv-common.c',
        '../src/ares/ares__close_sockets.c',
        '../src/ares/ares__get_hostent.c',
        '../src/ares/ares__read_line.c',
        '../src/ares/ares__timeval.c',
        '../src/ares/ares_cancel.c',
        '../src/ares/ares_data.c',
        '../src/ares/ares_destroy.c',
        '../src/ares/ares_expand_name.c',
        '../src/ares/ares_expand_string.c',
        '../src/ares/ares_fds.c',
        '../src/ares/ares_free_hostent.c',
        '../src/ares/ares_free_string.c',
        '../src/ares/ares_gethostbyaddr.c',
        '../src/ares/ares_gethostbyname.c',
        '../src/ares/ares_getnameinfo.c',
        '../src/ares/ares_getopt.c',
        '../src/ares/ares_getsock.c',
        '../src/ares/ares_init.c',
        '../src/ares/ares_library_init.c',
        '../src/ares/ares_llist.c',
        '../src/ares/ares_mkquery.c',
        '../src/ares/ares_nowarn.c',
        '../src/ares/ares_options.c',
        '../src/ares/ares_parse_a_reply.c',
        '../src/ares/ares_parse_aaaa_reply.c',
        '../src/ares/ares_parse_mx_reply.c',
        '../src/ares/ares_parse_ns_reply.c',
        '../src/ares/ares_parse_ptr_reply.c',
        '../src/ares/ares_parse_srv_reply.c',
        '../src/ares/ares_parse_txt_reply.c',
        '../src/ares/ares_process.c',
        '../src/ares/ares_query.c',
        '../src/ares/ares_search.c',
        '../src/ares/ares_send.c',
        '../src/ares/ares_strcasecmp.c',
        '../src/ares/ares_strdup.c',
        '../src/ares/ares_strerror.c',
        '../src/ares/ares_timeout.c',
        '../src/ares/ares_version.c',
        '../src/ares/ares_writev.c',
        '../src/ares/bitncmp.c',
        '../src/ares/inet_net_pton.c',
        '../src/ares/inet_ntop.c',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'dependencies': [
            '../deps/pthread-win32/build/all.gyp:pthread-win32'
          ],
          'export_dependent_settings': [
            '../deps/pthread-win32/build/all.gyp:pthread-win32'
          ],
          'include_dirs': [
            '../src/ares/config_win32'
          ],
          'sources': [ '../src/ares/windows_port.c' ],
          'defines': [
            '_WIN32_WINNT=0x0502',
            'EIO_STACKSIZE=262144',
            '_GNU_SOURCE',
          ],
          'sources': [
            '../src/win/async.c',
            '../src/win/cares.c',
            '../src/win/core.c',
            '../src/win/error.c',
            '../src/win/getaddrinfo.c',
            '../src/win/handle.c',
            '../src/win/loop-watcher.c',
            '../src/win/pipe.c',
            '../src/win/process.c',
            '../src/win/req.c',
            '../src/win/stdio.c',
            '../src/win/stream.c',
            '../src/win/tcp.c',
            '../src/win/timer.c',
            '../src/win/util.c',
          ]
        }, { # Not Windows i.e. POSIX
          'cflags': [
            '-g',
            '--std=gnu89',
            '-pedantic',
            '-Wall',
            '-Wextra',
            '-Wno-unused-parameter'
          ],
          'sources': [
            '../src/uv-eio.c',
            '../src/eio/eio.c',
            '../src/uv-unix.c',
            '../src/ev/ev.c',
          ],
          'include_dirs': [
            '../src/ev'
          ],
          'defines': [
            '_LARGEFILE_SOURCE',
            '_FILE_OFFSET_BITS=64',
            '_GNU_SOURCE',
            'EIO_STACKSIZE=262144'
          ],
          'libraries': [ '-lm' ]
        }],
        [ 'OS=="mac"', {
          'include_dirs': [ '../src/ares/config_darwin' ],
          'sources': [ '../src/uv-darwin.c' ],
          'direct_dependent_settings': {
            'libraries': [ '-framework CoreServices' ],
          },
          'defines': [
            'EV_CONFIG_H="config_darwin.h"',
            'EIO_CONFIG_H="config_darwin.h"',
          ]
        }],
        [ 'OS=="linux"', {
          'include_dirs': [ '../src/ares/config_linux' ],
          'sources': [ '../src/uv-linux.c' ],
          'defines': [
            'EV_CONFIG_H="config_linux.h"',
            'EIO_CONFIG_H="config_linux.h"',
          ],
          'direct_dependent_settings': {
            'libraries': [ '-lrt' ],
          },
        }],
        # TODO add OS=='sun'
      ]
    },

    {
      'target_name': 'run-tests',
      'type': 'executable',
      'dependencies': [ 'uv' ],
      'sources': [
        '../test/runner.c',
        '../test/run-tests.c',
        '../test/test-async.c',
        '../test/echo-server.c',
        '../test/test-callback-stack.c',
        '../test/test-connection-fail.c',
        '../test/test-delayed-accept.c',
        '../test/test-fail-always.c',
        '../test/test-get-currentexe.c',
        '../test/test-getaddrinfo.c',
        '../test/test-gethostbyname.c',
        '../test/test-getsockname.c',
        '../test/test-hrtime.c',
        '../test/test-idle.c',
        '../test/test-loop-handles.c',
        '../test/test-pass-always.c',
        '../test/test-ping-pong.c',
        '../test/test-pipe-bind-error.c',
        '../test/test-ref.c',
        '../test/test-shutdown-eof.c',
        '../test/test-spawn.c',
        '../test/test-tcp-bind-error.c',
        '../test/test-tcp-bind6-error.c',
        '../test/test-tcp-writealot.c',
        '../test/test-timer-again.c',
        '../test/test-timer.c',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [ '../test/runner-win.c' ],
          'libraries': [ 'ws2_32.lib' ]
        }, { # POSIX
          'defines': [ '_GNU_SOURCE' ],
          'ldflags': [ '-pthread' ],
          'sources': [ '../test/runner-unix.c' ] 
        }]
      ] 
    },

    {
      'target_name': 'run-benchmarks',
      'type': 'executable',
      'dependencies': [ 'uv' ],
      'sources': [
        '../test/runner.c',
        '../test/run-benchmarks.c',
        '../test/echo-server.c',
        '../test/dns-server.c',
        '../test/benchmark-ares.c',
        '../test/benchmark-getaddrinfo.c',
        '../test/benchmark-ping-pongs.c',
        '../test/benchmark-pound.c',
        '../test/benchmark-pump.c',
        '../test/benchmark-sizes.c',
        '../test/benchmark-spawn.c'
      ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [ '../test/runner-win.c' ],
          'libraries': [ 'ws2_32.lib' ]
        }, { # POSIX
          'defines': [ '_GNU_SOURCE' ],
          'ldflags': [ '-pthread' ],
          'sources': [ '../test/runner-unix.c' ] 
        }]
      ] 
    }
  ]
}

