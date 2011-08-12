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
          },
        },
      },
      'Release': {
        'defines': [ 'NDEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 0, # static release
          },
        },
      }
    },
    'msvs_settings': {
      'VCCLCompilerTool': {
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
          'WIN32'
        ],
      }]
    ],
  },

  'targets': [
    {
      'target_name': 'uv',
      'type': 'static_library',
      'include_dirs': [ 'include' ],
      'direct_dependent_settings': {
        'include_dirs': [ 'include' ],
      },

      'defines': [
        'HAVE_CONFIG_H'
      ],
      'sources': [
        'include/ares.h',
        'include/ares_version.h',
        'include/uv.h',
        'src/uv-common.c',
        'src/uv-common.h',
        'src/ares/ares__close_sockets.c',
        'src/ares/ares__get_hostent.c',
        'src/ares/ares__read_line.c',
        'src/ares/ares__timeval.c',
        'src/ares/ares_cancel.c',
        'src/ares/ares_data.c',
        'src/ares/ares_data.h',
        'src/ares/ares_destroy.c',
        'src/ares/ares_dns.h',
        'src/ares/ares_expand_name.c',
        'src/ares/ares_expand_string.c',
        'src/ares/ares_fds.c',
        'src/ares/ares_free_hostent.c',
        'src/ares/ares_free_string.c',
        'src/ares/ares_gethostbyaddr.c',
        'src/ares/ares_gethostbyname.c',
        'src/ares/ares_getnameinfo.c',
        'src/ares/ares_getopt.c',
        'src/ares/ares_getopt.h',
        'src/ares/ares_getsock.c',
        'src/ares/ares_init.c',
        'src/ares/ares_ipv6.h',
        'src/ares/ares_library_init.c',
        'src/ares/ares_library_init.h',
        'src/ares/ares_llist.c',
        'src/ares/ares_llist.h',
        'src/ares/ares_mkquery.c',
        'src/ares/ares_nowarn.c',
        'src/ares/ares_nowarn.h',
        'src/ares/ares_options.c',
        'src/ares/ares_parse_a_reply.c',
        'src/ares/ares_parse_aaaa_reply.c',
        'src/ares/ares_parse_mx_reply.c',
        'src/ares/ares_parse_ns_reply.c',
        'src/ares/ares_parse_ptr_reply.c',
        'src/ares/ares_parse_srv_reply.c',
        'src/ares/ares_parse_txt_reply.c',
        'src/ares/ares_private.h',
        'src/ares/ares_process.c',
        'src/ares/ares_query.c',
        'src/ares/ares_rules.h',
        'src/ares/ares_search.c',
        'src/ares/ares_send.c',
        'src/ares/ares_setup.h',
        'src/ares/ares_strcasecmp.c',
        'src/ares/ares_strcasecmp.h',
        'src/ares/ares_strdup.c',
        'src/ares/ares_strdup.h',
        'src/ares/ares_strerror.c',
        'src/ares/ares_timeout.c',
        'src/ares/ares_version.c',
        'src/ares/ares_writev.c',
        'src/ares/ares_writev.h',
        'src/ares/bitncmp.c',
        'src/ares/bitncmp.h',
        'src/ares/inet_net_pton.c',
        'src/ares/inet_net_pton.h',
        'src/ares/inet_ntop.c',
        'src/ares/inet_ntop.h',
        'src/ares/nameser.h',
        'src/ares/setup_once.h',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'include_dirs': [
            'src/ares/config_win32'
          ],
          'sources': [ 'src/ares/windows_port.c' ],
          'defines': [
            '_WIN32_WINNT=0x0502',
            'EIO_STACKSIZE=262144',
            '_GNU_SOURCE',
          ],
          'sources': [
            'include/tree.h',
            'include/uv-win.h',
            'src/ares/config_win32/ares_config.h',
            'src/win/async.c',
            'src/win/cares.c',
            'src/win/core.c',
            'src/win/error.c',
            'src/win/getaddrinfo.c',
            'src/win/handle.c',
            'src/win/internal.h',
            'src/win/loop-watcher.c',
            'src/win/ntdll.h',
            'src/win/pipe.c',
            'src/win/process.c',
            'src/win/req.c',
            'src/win/stdio.c',
            'src/win/stream.c',
            'src/win/tcp.c',
            'src/win/timer.c',
            'src/win/util.c',
            'src/win/winapi.c',
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
            'include/eio.h',
            'include/ev.h',
            'include/ngx-queue.h',
            'include/uv-unix.h',
            'src/uv-eio.c',
            'src/uv-eio.h',
            'src/uv-unix.c',
            'src/ares/config_cygwin/ares_config.h',
            'src/ares/config_darwin/ares_config.h',
            'src/ares/config_freebsd/ares_config.h',
            'src/ares/config_linux/ares_config.h',
            'src/ares/config_openbsd/ares_config.h',
            'src/ares/config_sunos/ares_config.h',
            'src/eio/config_cygwin.h',
            'src/eio/config_darwin.h',
            'src/eio/config_freebsd.h',
            'src/eio/config_linux.h',
            'src/eio/config_sunos.h',
            'src/eio/ecb.h',
            'src/eio/eio.c',
            'src/eio/xthread.h',
            'src/ev/config_cygwin.h',
            'src/ev/config_darwin.h',
            'src/ev/config_freebsd.h',
            'src/ev/config_linux.h',
            'src/ev/config_sunos.h',
            'src/ev/ev.c',
            'src/ev/ev_vars.h',
            'src/ev/ev_wrap.h',
            'src/ev/event.h',
          ],
          'include_dirs': [
            'src/ev'
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
          'include_dirs': [ 'src/ares/config_darwin' ],
          'sources': [ 'src/uv-darwin.c' ],
          'direct_dependent_settings': {
            'libraries': [ '-framework CoreServices' ],
          },
          'defines': [
            'EV_CONFIG_H="config_darwin.h"',
            'EIO_CONFIG_H="config_darwin.h"',
          ]
        }],
        [ 'OS=="linux"', {
          'include_dirs': [ 'src/ares/config_linux' ],
          'sources': [ 'src/uv-linux.c' ],
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
        'test/echo-server.c',
        'test/run-tests.c',
        'test/runner.c',
        'test/runner.h',
        'test/task.h',
        'test/test-async.c',
        'test/test-callback-stack.c',
        'test/test-connection-fail.c',
        'test/test-delayed-accept.c',
        'test/test-fail-always.c',
        'test/test-get-currentexe.c',
        'test/test-getaddrinfo.c',
        'test/test-gethostbyname.c',
        'test/test-getsockname.c',
        'test/test-hrtime.c',
        'test/test-idle.c',
        'test/test-list.h',
        'test/test-loop-handles.c',
        'test/test-pass-always.c',
        'test/test-ping-pong.c',
        'test/test-pipe-bind-error.c',
        'test/test-ref.c',
        'test/test-shutdown-eof.c',
        'test/test-spawn.c',
        'test/test-tcp-bind-error.c',
        'test/test-tcp-bind6-error.c',
        'test/test-tcp-writealot.c',
        'test/test-timer-again.c',
        'test/test-timer.c',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [
            'test/runner-win.c',
            'test/runner-win.h'
          ],
          'libraries': [ 'ws2_32.lib' ]
        }, { # POSIX
          'defines': [ '_GNU_SOURCE' ],
          'ldflags': [ '-pthread' ],
          'sources': [
            'test/runner-unix.c',
            'test/runner-unix.h',
          ]
        }]
      ]
    },

    {
      'target_name': 'run-benchmarks',
      'type': 'executable',
      'dependencies': [ 'uv' ],
      'sources': [
        'test/benchmark-ares.c',
        'test/benchmark-getaddrinfo.c',
        'test/benchmark-list.h',
        'test/benchmark-ping-pongs.c',
        'test/benchmark-pound.c',
        'test/benchmark-pump.c',
        'test/benchmark-sizes.c',
        'test/benchmark-spawn.c',
        'test/dns-server.c',
        'test/echo-server.c',
        'test/run-benchmarks.c',
        'test/runner.c',
        'test/runner.h',
        'test/task.h',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [
            'test/runner-win.c',
            'test/runner-win.h',
          ],
          'libraries': [ 'ws2_32.lib' ]
        }, { # POSIX
          'defines': [ '_GNU_SOURCE' ],
          'ldflags': [ '-pthread' ],
          'sources': [
            'test/runner-unix.c',
            'test/runner-unix.h',
          ]
        }]
      ]
    }
  ]
}

