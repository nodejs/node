{
  'target_defaults': {
    'conditions': [
      ['OS != "win"', {
        'defines': [
          '_LARGEFILE_SOURCE',
          '_FILE_OFFSET_BITS=64',
          '_GNU_SOURCE',
        ],
        'conditions': [
          ['OS=="solaris"', {
            'cflags': ['-pthreads'],
            'ldlags': ['-pthreads'],
          }, {
            'cflags': ['-pthread'],
            'ldlags': ['-pthread'],
          }],
        ],
      }],
    ],
  },

  'targets': [
    {
      'target_name': 'libuv',
      'type': '<(library)',
      'include_dirs': [
        'include',
        'include/uv-private',
        'src/',
      ],
      'direct_dependent_settings': {
        'include_dirs': [ 'include' ],
        'conditions': [
          ['OS=="linux"', {
            'libraries': [ '-ldl' ],
          }],
        ],
      },

      'sources': [
        'common.gypi',
        'include/uv.h',
        'include/uv-private/ngx-queue.h',
        'include/uv-private/tree.h',
        'src/fs-poll.c',
        'src/inet.c',
        'src/uv-common.c',
        'src/uv-common.h',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'defines': [
            '_WIN32_WINNT=0x0600',
            '_GNU_SOURCE',
          ],
          'sources': [
            'include/uv-private/uv-win.h',
            'src/win/async.c',
            'src/win/atomicops-inl.h',
            'src/win/core.c',
            'src/win/dl.c',
            'src/win/error.c',
            'src/win/fs.c',
            'src/win/fs-event.c',
            'src/win/getaddrinfo.c',
            'src/win/handle.c',
            'src/win/handle-inl.h',
            'src/win/internal.h',
            'src/win/loop-watcher.c',
            'src/win/pipe.c',
            'src/win/thread.c',
            'src/win/poll.c',
            'src/win/process.c',
            'src/win/process-stdio.c',
            'src/win/req.c',
            'src/win/req-inl.h',
            'src/win/signal.c',
            'src/win/stream.c',
            'src/win/stream-inl.h',
            'src/win/tcp.c',
            'src/win/tty.c',
            'src/win/threadpool.c',
            'src/win/timer.c',
            'src/win/udp.c',
            'src/win/util.c',
            'src/win/winapi.c',
            'src/win/winapi.h',
            'src/win/winsock.c',
            'src/win/winsock.h',
          ],
          'link_settings': {
            'libraries': [
              '-lws2_32.lib',
              '-lpsapi.lib',
              '-liphlpapi.lib'
            ],
          },
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
            'include/uv-private/ev.h',
            'include/uv-private/uv-unix.h',
            'include/uv-private/uv-linux.h',
            'include/uv-private/uv-sunos.h',
            'include/uv-private/uv-darwin.h',
            'include/uv-private/uv-bsd.h',
            'src/unix/async.c',
            'src/unix/core.c',
            'src/unix/dl.c',
            'src/unix/error.c',
            'src/unix/ev/ev.c',
            'src/unix/ev/ev_vars.h',
            'src/unix/ev/ev_wrap.h',
            'src/unix/ev/event.h',
            'src/unix/fs.c',
            'src/unix/getaddrinfo.c',
            'src/unix/internal.h',
            'src/unix/loop.c',
            'src/unix/loop-watcher.c',
            'src/unix/pipe.c',
            'src/unix/poll.c',
            'src/unix/process.c',
            'src/unix/signal.c',
            'src/unix/stream.c',
            'src/unix/tcp.c',
            'src/unix/thread.c',
            'src/unix/threadpool.c',
            'src/unix/timer.c',
            'src/unix/tty.c',
            'src/unix/udp.c',
          ],
          'include_dirs': [ 'src/unix/ev', ],
          'libraries': [ '-lm' ]
        }],
        [ 'OS=="mac"', {
          'sources': [ 'src/unix/darwin.c', 'src/unix/fsevents.c' ],
          'direct_dependent_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreServices.framework',
            ],
          },
          'defines': [
            '_DARWIN_USE_64_BIT_INODE=1',
            'EV_CONFIG_H="config_darwin.h"',
          ]
        }],
        [ 'OS=="linux"', {
          'sources': [
            'src/unix/linux/linux-core.c',
            'src/unix/linux/inotify.c',
            'src/unix/linux/syscalls.c',
            'src/unix/linux/syscalls.h',
          ],
          'defines': [
            'EV_CONFIG_H="config_linux.h"',
          ],
          'direct_dependent_settings': {
            'libraries': [ '-lrt' ],
          },
        }],
        [ 'OS=="solaris"', {
          'sources': [ 'src/unix/sunos.c' ],
          'defines': [
            '__EXTENSIONS__',
            '_XOPEN_SOURCE=500',
            'EV_CONFIG_H="config_sunos.h"',
          ],
          'direct_dependent_settings': {
            'libraries': [
              '-lkstat',
              '-lnsl',
              '-lsendfile',
              '-lsocket',
            ],
          },
        }],
        [ 'OS=="aix"', {
          'include_dirs': [ 'src/ares/config_aix' ],
          'sources': [ 'src/unix/aix.c' ],
          'defines': [
            '_ALL_SOURCE',
            '_XOPEN_SOURCE=500',
            'EV_CONFIG_H="config_aix.h"',
          ],
          'direct_dependent_settings': {
            'libraries': [
              '-lperfstat',
            ],
          },
        }],
        [ 'OS=="freebsd"', {
          'sources': [ 'src/unix/freebsd.c' ],
          'defines': [
            'EV_CONFIG_H="config_freebsd.h"',
          ],
          'direct_dependent_settings': {
            'libraries': [
              '-lkvm',
            ],
          },
        }],
        [ 'OS=="openbsd"', {
          'sources': [ 'src/unix/openbsd.c' ],
          'defines': [
            'EV_CONFIG_H="config_openbsd.h"',
          ],
        }],
        [ 'OS=="netbsd"', {
          'sources': [ 'src/unix/netbsd.c' ],
          'defines': [
            'EV_CONFIG_H="config_netbsd.h"',
          ],
          'direct_dependent_settings': {
            'libraries': [
              '-lkvm',
            ],
          },
        }],
        [ 'OS=="mac" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd"', {
          'sources': [ 'src/unix/kqueue.c' ],
        }],
        ['library=="shared_library"', {
          'defines': [ 'BUILDING_UV_SHARED=1' ]
        }]
      ]
    },

    {
      'target_name': 'run-tests',
      'type': 'executable',
      'dependencies': [ 'libuv' ],
      'sources': [
        'test/blackhole-server.c',
        'test/echo-server.c',
        'test/run-tests.c',
        'test/runner.c',
        'test/runner.h',
        'test/test-get-loadavg.c',
        'test/task.h',
        'test/test-util.c',
        'test/test-active.c',
        'test/test-async.c',
        'test/test-callback-stack.c',
        'test/test-callback-order.c',
        'test/test-connection-fail.c',
        'test/test-cwd-and-chdir.c',
        'test/test-delayed-accept.c',
        'test/test-error.c',
        'test/test-fail-always.c',
        'test/test-fs.c',
        'test/test-fs-event.c',
        'test/test-get-currentexe.c',
        'test/test-get-memory.c',
        'test/test-getaddrinfo.c',
        'test/test-getsockname.c',
        'test/test-hrtime.c',
        'test/test-idle.c',
        'test/test-ipc.c',
        'test/test-ipc-send-recv.c',
        'test/test-list.h',
        'test/test-loop-handles.c',
        'test/test-walk-handles.c',
        'test/test-multiple-listen.c',
        'test/test-pass-always.c',
        'test/test-ping-pong.c',
        'test/test-pipe-bind-error.c',
        'test/test-pipe-connect-error.c',
        'test/test-platform-output.c',
        'test/test-poll.c',
        'test/test-poll-close.c',
        'test/test-process-title.c',
        'test/test-ref.c',
        'test/test-run-once.c',
        'test/test-semaphore.c',
        'test/test-shutdown-close.c',
        'test/test-shutdown-eof.c',
        'test/test-signal.c',
        'test/test-signal-multiple-loops.c',
        'test/test-spawn.c',
        'test/test-fs-poll.c',
        'test/test-stdio-over-pipes.c',
        'test/test-tcp-bind-error.c',
        'test/test-tcp-bind6-error.c',
        'test/test-tcp-close.c',
        'test/test-tcp-close-while-connecting.c',
        'test/test-tcp-connect-error-after-write.c',
        'test/test-tcp-shutdown-after-write.c',
        'test/test-tcp-flags.c',
        'test/test-tcp-connect-error.c',
        'test/test-tcp-connect-timeout.c',
        'test/test-tcp-connect6-error.c',
        'test/test-tcp-open.c',
        'test/test-tcp-write-error.c',
        'test/test-tcp-write-to-half-open-connection.c',
        'test/test-tcp-writealot.c',
        'test/test-tcp-unexpected-read.c',
        'test/test-threadpool.c',
        'test/test-mutexes.c',
        'test/test-thread.c',
        'test/test-barrier.c',
        'test/test-condvar.c',
        'test/test-condvar-consumer-producer.c',
        'test/test-timer-again.c',
        'test/test-timer.c',
        'test/test-tty.c',
        'test/test-udp-dgram-too-big.c',
        'test/test-udp-ipv6.c',
        'test/test-udp-open.c',
        'test/test-udp-options.c',
        'test/test-udp-send-and-recv.c',
        'test/test-udp-multicast-join.c',
        'test/test-dlerror.c',
        'test/test-udp-multicast-ttl.c',
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
          'sources': [
            'test/runner-unix.c',
            'test/runner-unix.h',
          ],
        }],
        [ 'OS=="solaris"', { # make test-fs.c compile, needs _POSIX_C_SOURCE
          'defines': [
            '__EXTENSIONS__',
            '_XOPEN_SOURCE=500',
          ],
        }],
        [ 'OS=="aix"', {     # make test-fs.c compile, needs _POSIX_C_SOURCE
          'defines': [
            '_ALL_SOURCE',
            '_XOPEN_SOURCE=500',
          ],
        }],
      ],
      'msvs-settings': {
        'VCLinkerTool': {
          'SubSystem': 1, # /subsystem:console
        },
      },
    },

    {
      'target_name': 'run-benchmarks',
      'type': 'executable',
      'dependencies': [ 'libuv' ],
      'sources': [
        'test/benchmark-async.c',
        'test/benchmark-async-pummel.c',
        'test/benchmark-fs-stat.c',
        'test/benchmark-getaddrinfo.c',
        'test/benchmark-list.h',
        'test/benchmark-loop-count.c',
        'test/benchmark-million-timers.c',
        'test/benchmark-multi-accept.c',
        'test/benchmark-ping-pongs.c',
        'test/benchmark-pound.c',
        'test/benchmark-pump.c',
        'test/benchmark-sizes.c',
        'test/benchmark-spawn.c',
        'test/benchmark-thread.c',
        'test/benchmark-tcp-write-batch.c',
        'test/benchmark-udp-pummel.c',
        'test/dns-server.c',
        'test/echo-server.c',
        'test/blackhole-server.c',
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
          'sources': [
            'test/runner-unix.c',
            'test/runner-unix.h',
          ]
        }]
      ],
      'msvs-settings': {
        'VCLinkerTool': {
          'SubSystem': 1, # /subsystem:console
        },
      },
    }
  ]
}


