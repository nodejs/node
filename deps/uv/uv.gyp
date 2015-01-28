{
  'target_defaults': {
    'conditions': [
      ['OS != "win"', {
        'defines': [
          '_LARGEFILE_SOURCE',
          '_FILE_OFFSET_BITS=64',
        ],
        'conditions': [
          ['OS=="solaris"', {
            'cflags': [ '-pthreads' ],
          }],
          ['OS not in "solaris android"', {
            'cflags': [ '-pthread' ],
          }],
        ],
      }],
    ],
    'xcode_settings': {
      'WARNING_CFLAGS': [ '-Wall', '-Wextra', '-Wno-unused-parameter' ],
      'OTHER_CFLAGS': [ '-g', '--std=gnu89', '-pedantic' ],
    }
  },

  'targets': [
    {
      'target_name': 'libuv',
      'type': '<(uv_library)',
      'include_dirs': [
        'include',
        'src/',
      ],
      'direct_dependent_settings': {
        'include_dirs': [ 'include' ],
        'conditions': [
          ['OS != "win"', {
            'defines': [
              '_LARGEFILE_SOURCE',
              '_FILE_OFFSET_BITS=64',
            ],
          }],
          ['OS == "mac"', {
            'defines': [ '_DARWIN_USE_64_BIT_INODE=1' ],
          }],
          ['OS == "linux"', {
            'defines': [ '_POSIX_C_SOURCE=200112' ],
          }],
        ],
      },
      'sources': [
        'common.gypi',
        'include/uv.h',
        'include/tree.h',
        'include/uv-errno.h',
        'include/uv-threadpool.h',
        'include/uv-version.h',
        'src/fs-poll.c',
        'src/heap-inl.h',
        'src/inet.c',
        'src/queue.h',
        'src/threadpool.c',
        'src/uv-common.c',
        'src/uv-common.h',
        'src/version.c'
      ],
      'conditions': [
        [ 'OS=="win"', {
          'defines': [
            '_WIN32_WINNT=0x0600',
            '_GNU_SOURCE',
          ],
          'sources': [
            'include/uv-win.h',
            'src/win/async.c',
            'src/win/atomicops-inl.h',
            'src/win/core.c',
            'src/win/dl.c',
            'src/win/error.c',
            'src/win/fs.c',
            'src/win/fs-event.c',
            'src/win/getaddrinfo.c',
            'src/win/getnameinfo.c',
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
              '-ladvapi32',
              '-liphlpapi',
              '-lpsapi',
              '-lshell32',
              '-lws2_32'
            ],
          },
        }, { # Not Windows i.e. POSIX
          'cflags': [
            '-g',
            '--std=gnu89',
            '-pedantic',
            '-Wall',
            '-Wextra',
            '-Wno-unused-parameter',
          ],
          'sources': [
            'include/uv-unix.h',
            'include/uv-linux.h',
            'include/uv-sunos.h',
            'include/uv-darwin.h',
            'include/uv-bsd.h',
            'include/uv-aix.h',
            'src/unix/async.c',
            'src/unix/atomic-ops.h',
            'src/unix/core.c',
            'src/unix/dl.c',
            'src/unix/fs.c',
            'src/unix/getaddrinfo.c',
            'src/unix/getnameinfo.c',
            'src/unix/internal.h',
            'src/unix/loop.c',
            'src/unix/loop-watcher.c',
            'src/unix/pipe.c',
            'src/unix/poll.c',
            'src/unix/process.c',
            'src/unix/signal.c',
            'src/unix/spinlock.h',
            'src/unix/stream.c',
            'src/unix/tcp.c',
            'src/unix/thread.c',
            'src/unix/timer.c',
            'src/unix/tty.c',
            'src/unix/udp.c',
          ],
          'link_settings': {
            'libraries': [ '-lm' ],
            'conditions': [
              ['OS=="solaris"', {
                'ldflags': [ '-pthreads' ],
              }],
              ['OS != "solaris" and OS != "android"', {
                'ldflags': [ '-pthread' ],
              }],
            ],
          },
          'conditions': [
            ['uv_library=="shared_library"', {
              'cflags': [ '-fPIC' ],
            }],
            ['uv_library=="shared_library" and OS!="mac"', {
              'link_settings': {
                # Must correspond with UV_VERSION_MAJOR and UV_VERSION_MINOR
                # in include/uv-version.h
                'libraries': [ '-Wl,-soname,libuv.so.1.0' ],
              },
            }],
          ],
        }],
        [ 'OS in "linux mac android"', {
          'sources': [ 'src/unix/proctitle.c' ],
        }],
        [ 'OS=="mac"', {
          'sources': [
            'src/unix/darwin.c',
            'src/unix/fsevents.c',
            'src/unix/darwin-proctitle.c',
          ],
          'defines': [
            '_DARWIN_USE_64_BIT_INODE=1',
            '_DARWIN_UNLIMITED_SELECT=1',
          ]
        }],
        [ 'OS!="mac"', {
          # Enable on all platforms except OS X. The antique gcc/clang that
          # ships with Xcode emits waaaay too many false positives.
          'cflags': [ '-Wstrict-aliasing' ],
        }],
        [ 'OS=="linux"', {
          'defines': [ '_GNU_SOURCE' ],
          'sources': [
            'src/unix/linux-core.c',
            'src/unix/linux-inotify.c',
            'src/unix/linux-syscalls.c',
            'src/unix/linux-syscalls.h',
          ],
          'link_settings': {
            'libraries': [ '-ldl', '-lrt' ],
          },
        }],
        [ 'OS=="android"', {
          'sources': [
            'src/unix/linux-core.c',
            'src/unix/linux-inotify.c',
            'src/unix/linux-syscalls.c',
            'src/unix/linux-syscalls.h',
            'src/unix/pthread-fixes.c',
            'src/unix/android-ifaddrs.c'
          ],
          'link_settings': {
            'libraries': [ '-ldl' ],
          },
        }],
        [ 'OS=="solaris"', {
          'sources': [ 'src/unix/sunos.c' ],
          'defines': [
            '__EXTENSIONS__',
            '_XOPEN_SOURCE=500',
          ],
          'link_settings': {
            'libraries': [
              '-lkstat',
              '-lnsl',
              '-lsendfile',
              '-lsocket',
            ],
          },
        }],
        [ 'OS=="aix"', {
          'sources': [ 'src/unix/aix.c' ],
          'defines': [
            '_ALL_SOURCE',
            '_XOPEN_SOURCE=500',
            '_LINUX_SOURCE_COMPAT',
          ],
          'link_settings': {
            'libraries': [
              '-lperfstat',
            ],
          },
        }],
        [ 'OS=="freebsd" or OS=="dragonflybsd"', {
          'sources': [ 'src/unix/freebsd.c' ],
        }],
        [ 'OS=="openbsd"', {
          'sources': [ 'src/unix/openbsd.c' ],
        }],
        [ 'OS=="netbsd"', {
          'sources': [ 'src/unix/netbsd.c' ],
        }],
        [ 'OS in "freebsd dragonflybsd openbsd netbsd".split()', {
          'link_settings': {
            'libraries': [ '-lkvm' ],
          },
        }],
        [ 'OS in "mac freebsd dragonflybsd openbsd netbsd".split()', {
          'sources': [ 'src/unix/kqueue.c' ],
        }],
        ['uv_library=="shared_library"', {
          'defines': [ 'BUILDING_UV_SHARED=1' ]
        }],
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
        'test/test-active.c',
        'test/test-async.c',
        'test/test-async-null-cb.c',
        'test/test-callback-stack.c',
        'test/test-callback-order.c',
        'test/test-close-fd.c',
        'test/test-close-order.c',
        'test/test-connection-fail.c',
        'test/test-cwd-and-chdir.c',
        'test/test-default-loop-close.c',
        'test/test-delayed-accept.c',
        'test/test-error.c',
        'test/test-embed.c',
        'test/test-emfile.c',
        'test/test-fail-always.c',
        'test/test-fs.c',
        'test/test-fs-event.c',
        'test/test-get-currentexe.c',
        'test/test-get-memory.c',
        'test/test-getaddrinfo.c',
        'test/test-getnameinfo.c',
        'test/test-getsockname.c',
        'test/test-handle-fileno.c',
        'test/test-hrtime.c',
        'test/test-idle.c',
        'test/test-ip6-addr.c',
        'test/test-ipc.c',
        'test/test-ipc-send-recv.c',
        'test/test-list.h',
        'test/test-loop-handles.c',
        'test/test-loop-alive.c',
        'test/test-loop-close.c',
        'test/test-loop-stop.c',
        'test/test-loop-time.c',
        'test/test-loop-configure.c',
        'test/test-walk-handles.c',
        'test/test-watcher-cross-stop.c',
        'test/test-multiple-listen.c',
        'test/test-osx-select.c',
        'test/test-pass-always.c',
        'test/test-ping-pong.c',
        'test/test-pipe-bind-error.c',
        'test/test-pipe-connect-error.c',
        'test/test-pipe-getsockname.c',
        'test/test-pipe-sendmsg.c',
        'test/test-pipe-server-close.c',
        'test/test-pipe-close-stdout-read-stdin.c',
        'test/test-platform-output.c',
        'test/test-poll.c',
        'test/test-poll-close.c',
        'test/test-poll-close-doesnt-corrupt-stack.c',
        'test/test-poll-closesocket.c',
        'test/test-process-title.c',
        'test/test-ref.c',
        'test/test-run-nowait.c',
        'test/test-run-once.c',
        'test/test-semaphore.c',
        'test/test-shutdown-close.c',
        'test/test-shutdown-eof.c',
        'test/test-shutdown-twice.c',
        'test/test-signal.c',
        'test/test-signal-multiple-loops.c',
        'test/test-socket-buffer-size.c',
        'test/test-spawn.c',
        'test/test-fs-poll.c',
        'test/test-stdio-over-pipes.c',
        'test/test-tcp-bind-error.c',
        'test/test-tcp-bind6-error.c',
        'test/test-tcp-close.c',
        'test/test-tcp-close-accept.c',
        'test/test-tcp-close-while-connecting.c',
        'test/test-tcp-connect-error-after-write.c',
        'test/test-tcp-shutdown-after-write.c',
        'test/test-tcp-flags.c',
        'test/test-tcp-connect-error.c',
        'test/test-tcp-connect-timeout.c',
        'test/test-tcp-connect6-error.c',
        'test/test-tcp-open.c',
        'test/test-tcp-write-to-half-open-connection.c',
        'test/test-tcp-write-after-connect.c',
        'test/test-tcp-writealot.c',
        'test/test-tcp-try-write.c',
        'test/test-tcp-unexpected-read.c',
        'test/test-tcp-read-stop.c',
        'test/test-tcp-write-queue-order.c',
        'test/test-threadpool.c',
        'test/test-threadpool-cancel.c',
        'test/test-thread-equal.c',
        'test/test-mutexes.c',
        'test/test-thread.c',
        'test/test-barrier.c',
        'test/test-condvar.c',
        'test/test-timer-again.c',
        'test/test-timer-from-check.c',
        'test/test-timer.c',
        'test/test-tty.c',
        'test/test-udp-bind.c',
        'test/test-udp-dgram-too-big.c',
        'test/test-udp-ipv6.c',
        'test/test-udp-open.c',
        'test/test-udp-options.c',
        'test/test-udp-send-and-recv.c',
        'test/test-udp-send-immediate.c',
        'test/test-udp-send-unreachable.c',
        'test/test-udp-multicast-join.c',
        'test/test-udp-multicast-join6.c',
        'test/test-dlerror.c',
        'test/test-udp-multicast-ttl.c',
        'test/test-ip4-addr.c',
        'test/test-ip6-addr.c',
        'test/test-udp-multicast-interface.c',
        'test/test-udp-multicast-interface6.c',
        'test/test-udp-try-send.c',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [
            'test/runner-win.c',
            'test/runner-win.h'
          ],
          'libraries': [ '-lws2_32' ]
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
        'test/benchmark-million-async.c',
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
          'libraries': [ '-lws2_32' ]
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
    },
  ]
}
