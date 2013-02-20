#!/bin/sh

# Copyright (c) 2013, Ben Noordhuis <info@bnoordhuis.nl>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

SPARSE=${SPARSE:-sparse}

SPARSE_FLAGS=${SPARSE_FLAGS:-"
-D__POSIX__
-Wsparse-all
-Wno-do-while
-Wno-transparent-union
-Iinclude
-Iinclude/uv-private
-Isrc
"}

SOURCES="
include/uv-private/ngx-queue.h
include/uv-private/tree.h
include/uv-private/uv-unix.h
include/uv.h
src/fs-poll.c
src/inet.c
src/unix/async.c
src/unix/core.c
src/unix/dl.c
src/unix/error.c
src/unix/fs.c
src/unix/getaddrinfo.c
src/unix/internal.h
src/unix/loop-watcher.c
src/unix/loop.c
src/unix/pipe.c
src/unix/poll.c
src/unix/process.c
src/unix/signal.c
src/unix/stream.c
src/unix/tcp.c
src/unix/thread.c
src/unix/threadpool.c
src/unix/timer.c
src/unix/tty.c
src/unix/udp.c
src/uv-common.c
src/uv-common.h
"

TESTS="
test/benchmark-async-pummel.c
test/benchmark-async.c
test/benchmark-fs-stat.c
test/benchmark-getaddrinfo.c
test/benchmark-loop-count.c
test/benchmark-million-async.c
test/benchmark-million-timers.c
test/benchmark-multi-accept.c
test/benchmark-ping-pongs.c
test/benchmark-pound.c
test/benchmark-pump.c
test/benchmark-sizes.c
test/benchmark-spawn.c
test/benchmark-tcp-write-batch.c
test/benchmark-thread.c
test/benchmark-udp-pummel.c
test/blackhole-server.c
test/dns-server.c
test/echo-server.c
test/run-benchmarks.c
test/run-tests.c
test/runner-unix.c
test/runner-unix.h
test/runner.c
test/runner.h
test/task.h
test/test-active.c
test/test-async.c
test/test-barrier.c
test/test-callback-order.c
test/test-callback-stack.c
test/test-condvar.c
test/test-connection-fail.c
test/test-cwd-and-chdir.c
test/test-delayed-accept.c
test/test-dlerror.c
test/test-embed.c
test/test-error.c
test/test-fail-always.c
test/test-fs-event.c
test/test-fs-poll.c
test/test-fs.c
test/test-get-currentexe.c
test/test-get-loadavg.c
test/test-get-memory.c
test/test-getaddrinfo.c
test/test-getsockname.c
test/test-hrtime.c
test/test-idle.c
test/test-ipc-send-recv.c
test/test-ipc.c
test/test-loop-handles.c
test/test-multiple-listen.c
test/test-mutexes.c
test/test-pass-always.c
test/test-ping-pong.c
test/test-pipe-bind-error.c
test/test-pipe-connect-error.c
test/test-platform-output.c
test/test-poll-close.c
test/test-poll.c
test/test-process-title.c
test/test-ref.c
test/test-run-nowait.c
test/test-run-once.c
test/test-semaphore.c
test/test-shutdown-close.c
test/test-shutdown-eof.c
test/test-signal-multiple-loops.c
test/test-signal.c
test/test-spawn.c
test/test-stdio-over-pipes.c
test/test-tcp-bind-error.c
test/test-tcp-bind6-error.c
test/test-tcp-close-while-connecting.c
test/test-tcp-close.c
test/test-tcp-connect-error-after-write.c
test/test-tcp-connect-error.c
test/test-tcp-connect-timeout.c
test/test-tcp-connect6-error.c
test/test-tcp-flags.c
test/test-tcp-open.c
test/test-tcp-read-stop.c
test/test-tcp-shutdown-after-write.c
test/test-tcp-unexpected-read.c
test/test-tcp-write-error.c
test/test-tcp-write-to-half-open-connection.c
test/test-tcp-writealot.c
test/test-thread.c
test/test-threadpool-cancel.c
test/test-threadpool.c
test/test-timer-again.c
test/test-timer.c
test/test-tty.c
test/test-udp-dgram-too-big.c
test/test-udp-ipv6.c
test/test-udp-multicast-join.c
test/test-udp-multicast-ttl.c
test/test-udp-open.c
test/test-udp-options.c
test/test-udp-send-and-recv.c
test/test-util.c
test/test-walk-handles.c
"

case `uname -s` in
AIX)
  SPARSE_FLAGS="$SPARSE_FLAGS -D_AIX=1"
  SOURCES="$SOURCES
           src/unix/aix.c"
  ;;
Darwin)
  SPARSE_FLAGS="$SPARSE_FLAGS -D__APPLE__=1"
  SOURCES="$SOURCES
           include/uv-private/uv-bsd.h
           src/unix/darwin.c
           src/unix/kqueue.c
           src/unix/fsevents.c"
  ;;
DragonFly)
  SPARSE_FLAGS="$SPARSE_FLAGS -D__DragonFly__=1"
  SOURCES="$SOURCES
           include/uv-private/uv-bsd.h
           src/unix/kqueue.c
           src/unix/freebsd.c"
  ;;
FreeBSD)
  SPARSE_FLAGS="$SPARSE_FLAGS -D__FreeBSD__=1"
  SOURCES="$SOURCES
           include/uv-private/uv-bsd.h
           src/unix/kqueue.c
           src/unix/freebsd.c"
  ;;
Linux)
  SPARSE_FLAGS="$SPARSE_FLAGS -D__linux__=1"
  SOURCES="$SOURCES
           include/uv-private/uv-linux.h
           src/unix/linux-inotify.c
           src/unix/linux-core.c
           src/unix/linux-syscalls.c
           src/unix/linux-syscalls.h"
  ;;
NetBSD)
  SPARSE_FLAGS="$SPARSE_FLAGS -D__NetBSD__=1"
  SOURCES="$SOURCES
           include/uv-private/uv-bsd.h
           src/unix/kqueue.c
           src/unix/netbsd.c"
  ;;
OpenBSD)
  SPARSE_FLAGS="$SPARSE_FLAGS -D__OpenBSD__=1"
  SOURCES="$SOURCES
           include/uv-private/uv-bsd.h
           src/unix/kqueue.c
           src/unix/openbsd.c"
  ;;
SunOS)
  SPARSE_FLAGS="$SPARSE_FLAGS -D__sun=1"
  SOURCES="$SOURCES
           include/uv-private/uv-sunos.h
           src/unix/sunos.c"
  ;;
esac

for ARCH in __i386__ __x86_64__ __arm__; do
  $SPARSE $SPARSE_FLAGS -D$ARCH=1 $SOURCES
done

# Tests are architecture independent.
$SPARSE $SPARSE_FLAGS -Itest $TESTS
