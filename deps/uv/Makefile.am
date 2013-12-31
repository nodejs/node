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

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(top_srcdir)/include \
              -I$(top_srcdir)/src

include_HEADERS=include/uv.h include/uv-errno.h

CLEANFILES =

lib_LTLIBRARIES = libuv.la
libuv_la_CFLAGS = @CFLAGS@
libuv_la_LDFLAGS = -no-undefined -version-info 11:0:0
libuv_la_SOURCES = src/fs-poll.c \
                   src/inet.c \
                   src/queue.h \
                   src/uv-common.c \
                   src/uv-common.h \
                   src/version.c

if SUNOS
libuv_la_CFLAGS += -pthread
endif

if WINNT

include_HEADERS += include/uv-win.h include/tree.h
AM_CPPFLAGS += -I$(top_srcdir)/src/win \
               -DWIN32_LEAN_AND_MEAN \
               -D_WIN32_WINNT=0x0600
LIBS += -lws2_32 -lpsapi -liphlpapi -lshell32
libuv_la_SOURCES += src/win/async.c \
                    src/win/atomicops-inl.h \
                    src/win/core.c \
                    src/win/dl.c \
                    src/win/error.c \
                    src/win/fs-event.c \
                    src/win/fs.c \
                    src/win/getaddrinfo.c \
                    src/win/handle.c \
                    src/win/handle-inl.h \
                    src/win/internal.h \
                    src/win/loop-watcher.c \
                    src/win/pipe.c \
                    src/win/poll.c \
                    src/win/process-stdio.c \
                    src/win/process.c \
                    src/win/req.c \
                    src/win/req-inl.h \
                    src/win/signal.c \
                    src/win/stream.c \
                    src/win/stream-inl.h \
                    src/win/tcp.c \
                    src/win/thread.c \
                    src/win/threadpool.c \
                    src/win/timer.c \
                    src/win/tty.c \
                    src/win/udp.c \
                    src/win/util.c \
                    src/win/winapi.c \
                    src/win/winapi.h \
                    src/win/winsock.c \
                    src/win/winsock.h

else  # WINNT

include_HEADERS += include/uv-unix.h
AM_CPPFLAGS += -I$(top_srcdir)/src/unix
libuv_la_SOURCES += src/unix/async.c \
                   src/unix/atomic-ops.h \
                   src/unix/core.c \
                   src/unix/dl.c \
                   src/unix/fs.c \
                   src/unix/getaddrinfo.c \
                   src/unix/internal.h \
                   src/unix/loop-watcher.c \
                   src/unix/loop.c \
                   src/unix/pipe.c \
                   src/unix/poll.c \
                   src/unix/process.c \
                   src/unix/signal.c \
                   src/unix/spinlock.h \
                   src/unix/stream.c \
                   src/unix/tcp.c \
                   src/unix/thread.c \
                   src/unix/threadpool.c \
                   src/unix/timer.c \
                   src/unix/tty.c \
                   src/unix/udp.c

endif  # WINNT

TESTS = test/run-tests
check_PROGRAMS = test/run-tests
test_run_tests_SOURCES = test/blackhole-server.c \
                         test/dns-server.c \
                         test/echo-server.c \
                         test/run-tests.c \
                         test/runner.c \
                         test/runner.h \
                         test/task.h \
                         test/test-active.c \
                         test/test-async.c \
                         test/test-async-null-cb.c \
                         test/test-barrier.c \
                         test/test-callback-order.c \
                         test/test-callback-stack.c \
                         test/test-close-fd.c \
                         test/test-close-order.c \
                         test/test-condvar.c \
                         test/test-connection-fail.c \
                         test/test-cwd-and-chdir.c \
                         test/test-delayed-accept.c \
                         test/test-dlerror.c \
                         test/test-embed.c \
                         test/test-emfile.c \
                         test/test-error.c \
                         test/test-fail-always.c \
                         test/test-fs-event.c \
                         test/test-fs-poll.c \
                         test/test-fs.c \
                         test/test-get-currentexe.c \
                         test/test-get-loadavg.c \
                         test/test-get-memory.c \
                         test/test-getaddrinfo.c \
                         test/test-getsockname.c \
                         test/test-hrtime.c \
                         test/test-idle.c \
                         test/test-ip4-addr.c \
                         test/test-ip6-addr.c \
                         test/test-ipc-send-recv.c \
                         test/test-ipc.c \
                         test/test-list.h \
                         test/test-loop-handles.c \
                         test/test-loop-alive.c \
                         test/test-loop-stop.c \
                         test/test-loop-time.c \
                         test/test-multiple-listen.c \
                         test/test-mutexes.c \
                         test/test-osx-select.c \
                         test/test-pass-always.c \
                         test/test-ping-pong.c \
                         test/test-pipe-bind-error.c \
                         test/test-pipe-connect-error.c \
                         test/test-pipe-server-close.c \
                         test/test-platform-output.c \
                         test/test-poll-close.c \
                         test/test-poll.c \
                         test/test-process-title.c \
                         test/test-ref.c \
                         test/test-run-nowait.c \
                         test/test-run-once.c \
                         test/test-semaphore.c \
                         test/test-shutdown-close.c \
                         test/test-shutdown-eof.c \
                         test/test-signal-multiple-loops.c \
                         test/test-signal.c \
                         test/test-spawn.c \
                         test/test-stdio-over-pipes.c \
                         test/test-tcp-bind-error.c \
                         test/test-tcp-bind6-error.c \
                         test/test-tcp-close-accept.c \
                         test/test-tcp-close-while-connecting.c \
                         test/test-tcp-close.c \
                         test/test-tcp-connect-error-after-write.c \
                         test/test-tcp-connect-error.c \
                         test/test-tcp-connect-timeout.c \
                         test/test-tcp-connect6-error.c \
                         test/test-tcp-flags.c \
                         test/test-tcp-open.c \
                         test/test-tcp-read-stop.c \
                         test/test-tcp-shutdown-after-write.c \
                         test/test-tcp-unexpected-read.c \
                         test/test-tcp-write-to-half-open-connection.c \
                         test/test-tcp-writealot.c \
                         test/test-tcp-try-write.c \
                         test/test-thread.c \
                         test/test-threadpool-cancel.c \
                         test/test-threadpool.c \
                         test/test-timer-again.c \
                         test/test-timer-from-check.c \
                         test/test-timer.c \
                         test/test-tty.c \
                         test/test-udp-dgram-too-big.c \
                         test/test-udp-ipv6.c \
                         test/test-udp-multicast-join.c \
                         test/test-udp-multicast-ttl.c \
                         test/test-udp-open.c \
                         test/test-udp-options.c \
                         test/test-udp-send-and-recv.c \
                         test/test-walk-handles.c \
                         test/test-watcher-cross-stop.c
test_run_tests_LDADD = libuv.la

if WINNT
test_run_tests_SOURCES += test/runner-win.c \
                          test/runner-win.h
else
test_run_tests_SOURCES += test/runner-unix.c \
                          test/runner-unix.h
endif



if AIX
libuv_la_CFLAGS += -D_ALL_SOURCE -D_XOPEN_SOURCE=500
libuv_la_SOURCES += src/unix/aix.c
endif

if DARWIN
include_HEADERS += include/uv-darwin.h
libuv_la_CFLAGS += -D_DARWIN_USE_64_BIT_INODE=1
libuv_la_SOURCES += src/unix/darwin.c \
                    src/unix/darwin-proctitle.c \
                    src/unix/fsevents.c \
                    src/unix/kqueue.c \
                    src/unix/proctitle.c
endif

if FREEBSD
include_HEADERS += include/uv-bsd.h
libuv_la_SOURCES += src/unix/freebsd.c src/unix/kqueue.c
endif

if LINUX
include_HEADERS += include/uv-linux.h
libuv_la_SOURCES += src/unix/linux-core.c \
                    src/unix/linux-inotify.c \
                    src/unix/linux-syscalls.c \
                    src/unix/linux-syscalls.h \
                    src/unix/proctitle.c
endif

if NETBSD
include_HEADERS += include/uv-bsd.h
libuv_la_SOURCES += src/unix/kqueue.c src/unix/netbsd.c
endif

if OPENBSD
include_HEADERS += include/uv-bsd.h
libuv_la_SOURCES += src/unix/kqueue.c src/unix/openbsd.c
endif

if SUNOS
include_HEADERS += include/uv-sunos.h
libuv_la_CFLAGS += -D__EXTENSIONS__ -D_XOPEN_SOURCE=500
libuv_la_SOURCES += src/unix/sunos.c
endif

if HAVE_DTRACE
BUILT_SOURCES = include/uv-dtrace.h
CLEANFILES += include/uv-dtrace.h
endif

if DTRACE_NEEDS_OBJECTS
libuv_la_SOURCES += src/unix/uv-dtrace.d
libuv_la_DEPENDENCIES = src/unix/uv-dtrace.o
libuv_la_LIBADD = uv-dtrace.lo
CLEANFILES += src/unix/uv-dtrace.o src/unix/uv-dtrace.lo
endif

if HAVE_PKG_CONFIG
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = @PACKAGE_NAME@.pc
endif

if HAVE_DTRACE
include/uv-dtrace.h: src/unix/uv-dtrace.d
	$(AM_V_GEN)$(DTRACE) $(DTRACEFLAGS) -h -xnolibs -s $< -o $(top_srcdir)/$@
endif

if DTRACE_NEEDS_OBJECTS
SUFFIXES = .d

src/unix/uv-dtrace.o: src/unix/uv-dtrace.d ${libuv_la_OBJECTS}

# It's ok to specify the output here, because we have 1 .d file, and we process
# every created .o, most projects don't need to include more than one .d
.d.o:
	$(AM_V_GEN)$(DTRACE) $(DTRACEFLAGS) -G -o $(top_builddir)/uv-dtrace.o -s $< \
		`find ${top_builddir}/src -name "*.o"`
	$(AM_V_GEN)printf %s\\n \
		'# ${top_builddir}/uv-dtrace.lo - a libtool object file' \
		'# Generated by libtool (GNU libtool) 2.4' \
		'# libtool wants a .lo not a .o' \
		"pic_object='uv-dtrace.o'" \
		"non_pic_object='uv-dtrace.o'" \
	> ${top_builddir}/uv-dtrace.lo
endif
