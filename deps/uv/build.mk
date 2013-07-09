# Copyright Joyent, Inc. and other Node contributors. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

ifdef PLATFORM
override PLATFORM := $(shell echo $(PLATFORM) | tr "[A-Z]" "[a-z]")
else
PLATFORM = $(shell sh -c 'uname -s | tr "[A-Z]" "[a-z]"')
endif

CPPFLAGS += -I$(SRCDIR)/include -I$(SRCDIR)/include/uv-private

ifeq (darwin,$(PLATFORM))
SOEXT = dylib
else
SOEXT = so
endif

ifneq (,$(findstring mingw,$(PLATFORM)))
include $(SRCDIR)/config-mingw.mk
else
include $(SRCDIR)/config-unix.mk
endif

BENCHMARKS= \
	test/benchmark-async-pummel.o \
	test/benchmark-async.o \
	test/benchmark-fs-stat.o \
	test/benchmark-getaddrinfo.o \
	test/benchmark-loop-count.o \
	test/benchmark-million-async.o \
	test/benchmark-million-timers.o \
	test/benchmark-multi-accept.o \
	test/benchmark-ping-pongs.o \
	test/benchmark-pound.o \
	test/benchmark-pump.o \
	test/benchmark-sizes.o \
	test/benchmark-spawn.o \
	test/benchmark-tcp-write-batch.o \
	test/benchmark-thread.o \
	test/benchmark-udp-pummel.o \
	test/blackhole-server.o \
	test/dns-server.o \
	test/echo-server.o \

TESTS= \
	test/blackhole-server.o \
	test/dns-server.o \
	test/echo-server.o \
	test/test-active.o \
	test/test-async.o \
	test/test-barrier.o \
	test/test-callback-order.o \
	test/test-callback-stack.o \
	test/test-condvar.o \
	test/test-connection-fail.o \
	test/test-cwd-and-chdir.o \
	test/test-delayed-accept.o \
	test/test-dlerror.o \
	test/test-embed.o \
	test/test-error.o \
	test/test-fail-always.o \
	test/test-fs.o \
	test/test-fs-event.o \
	test/test-fs-poll.o \
	test/test-getaddrinfo.o \
	test/test-get-currentexe.o \
	test/test-get-loadavg.o \
	test/test-get-memory.o \
	test/test-getsockname.o \
	test/test-hrtime.o \
	test/test-idle.o \
	test/test-ipc.o \
	test/test-ipc-send-recv.o \
	test/test-loop-handles.o \
	test/test-loop-stop.o \
	test/test-multiple-listen.o \
	test/test-mutexes.o \
	test/test-osx-select.o \
	test/test-pass-always.o \
	test/test-ping-pong.o \
	test/test-pipe-bind-error.o \
	test/test-pipe-connect-error.o \
	test/test-platform-output.o \
	test/test-poll.o \
	test/test-poll-close.o \
	test/test-process-title.o \
	test/test-ref.o \
	test/test-run-nowait.o \
	test/test-run-once.o \
	test/test-semaphore.o \
	test/test-shutdown-close.o \
	test/test-shutdown-eof.o \
	test/test-signal.o \
	test/test-signal-multiple-loops.o \
	test/test-spawn.o \
	test/test-stdio-over-pipes.o \
	test/test-tcp-bind6-error.o \
	test/test-tcp-bind-error.o \
	test/test-tcp-close.o \
	test/test-tcp-close-while-connecting.o \
	test/test-tcp-connect6-error.o \
	test/test-tcp-connect-error-after-write.o \
	test/test-tcp-connect-error.o \
	test/test-tcp-connect-timeout.o \
	test/test-tcp-flags.o \
	test/test-tcp-open.o \
	test/test-tcp-read-stop.o \
	test/test-tcp-shutdown-after-write.o \
	test/test-tcp-unexpected-read.o \
	test/test-tcp-writealot.o \
	test/test-tcp-write-to-half-open-connection.o \
	test/test-thread.o \
	test/test-threadpool.o \
	test/test-threadpool-cancel.o \
	test/test-timer-again.o \
	test/test-timer-from-check.o \
	test/test-timer.o \
	test/test-tty.o \
	test/test-udp-dgram-too-big.o \
	test/test-udp-ipv6.o \
	test/test-udp-multicast-join.o \
	test/test-udp-multicast-ttl.o \
	test/test-udp-open.o \
	test/test-udp-options.o \
	test/test-udp-send-and-recv.o \
	test/test-util.o \
	test/test-walk-handles.o \

.PHONY: all bench clean clean-platform distclean test

run-tests$(E): test/run-tests.o test/runner.o $(RUNNER_SRC) $(TESTS) libuv.a
	$(CC) $(CPPFLAGS) $(RUNNER_CFLAGS) -o $@ $^ $(RUNNER_LIBS) $(RUNNER_LDFLAGS)

run-benchmarks$(E): test/run-benchmarks.o test/runner.o $(RUNNER_SRC) $(BENCHMARKS) libuv.a
	$(CC) $(CPPFLAGS) $(RUNNER_CFLAGS) -o $@ $^ $(RUNNER_LIBS) $(RUNNER_LDFLAGS)

test/echo.o: test/echo.c test/echo.h

test: run-tests$(E)
	$(CURDIR)/$<

bench: run-benchmarks$(E)
	$(CURDIR)/$<

clean distclean: clean-platform
	$(RM) libuv.a libuv.$(SOEXT) \
		test/run-tests.o test/run-benchmarks.o \
		test/run-tests$(E) test/run-benchmarks$(E) \
		$(BENCHMARKS) $(TESTS) $(RUNNER_LIBS)
