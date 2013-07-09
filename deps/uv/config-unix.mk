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

E=
CSTDFLAG=--std=c89 -pedantic -Wall -Wextra -Wno-unused-parameter
CFLAGS += -g
CPPFLAGS += -I$(SRCDIR)/src
LDFLAGS=-lm

CPPFLAGS += -D_LARGEFILE_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64

RUNNER_SRC=test/runner-unix.c
RUNNER_CFLAGS=$(CFLAGS) -I$(SRCDIR)/test
RUNNER_LDFLAGS=

DTRACE_OBJS=
DTRACE_HEADER=

OBJS += src/unix/async.o
OBJS += src/unix/core.o
OBJS += src/unix/dl.o
OBJS += src/unix/error.o
OBJS += src/unix/fs.o
OBJS += src/unix/getaddrinfo.o
OBJS += src/unix/loop.o
OBJS += src/unix/loop-watcher.o
OBJS += src/unix/pipe.o
OBJS += src/unix/poll.o
OBJS += src/unix/process.o
OBJS += src/unix/signal.o
OBJS += src/unix/stream.o
OBJS += src/unix/tcp.o
OBJS += src/unix/thread.o
OBJS += src/unix/threadpool.o
OBJS += src/unix/timer.o
OBJS += src/unix/tty.o
OBJS += src/unix/udp.o
OBJS += src/fs-poll.o
OBJS += src/uv-common.o
OBJS += src/inet.o
OBJS += src/version.o

ifeq (sunos,$(PLATFORM))
HAVE_DTRACE ?= 1
CPPFLAGS += -D__EXTENSIONS__ -D_XOPEN_SOURCE=500
LDFLAGS+=-lkstat -lnsl -lsendfile -lsocket
# Library dependencies are not transitive.
OBJS += src/unix/sunos.o
ifeq (1, $(HAVE_DTRACE))
OBJS += src/unix/dtrace.o
DTRACE_OBJS += src/unix/core.o
endif
endif

ifeq (aix,$(PLATFORM))
CPPFLAGS += -D_ALL_SOURCE -D_XOPEN_SOURCE=500
LDFLAGS+= -lperfstat
OBJS += src/unix/aix.o
endif

ifeq (darwin,$(PLATFORM))
HAVE_DTRACE ?= 1
# dtrace(1) probes contain dollar signs on OS X. Mute the warnings they
# generate but only when CC=clang, -Wno-dollar-in-identifier-extension
# is a clang extension.
ifeq (__clang__,$(shell sh -c "$(CC) -dM -E - </dev/null | grep -ow __clang__"))
CFLAGS += -Wno-dollar-in-identifier-extension
endif
CPPFLAGS += -D_DARWIN_USE_64_BIT_INODE=1
LDFLAGS += -framework Foundation \
           -framework CoreServices \
           -framework ApplicationServices
SOEXT = dylib
OBJS += src/unix/darwin.o
OBJS += src/unix/kqueue.o
OBJS += src/unix/fsevents.o
OBJS += src/unix/proctitle.o
OBJS += src/unix/darwin-proctitle.o
endif

ifeq (linux,$(PLATFORM))
CSTDFLAG += -D_GNU_SOURCE
LDFLAGS+=-ldl -lrt
RUNNER_CFLAGS += -D_GNU_SOURCE
OBJS += src/unix/linux-core.o \
        src/unix/linux-inotify.o \
        src/unix/linux-syscalls.o \
        src/unix/proctitle.o
endif

ifeq (freebsd,$(PLATFORM))
ifeq ($(shell dtrace -l 1>&2 2>/dev/null; echo $$?),0)
HAVE_DTRACE ?= 1
endif
LDFLAGS+=-lkvm
OBJS += src/unix/freebsd.o
OBJS += src/unix/kqueue.o
endif

ifeq (dragonfly,$(PLATFORM))
LDFLAGS+=-lkvm
OBJS += src/unix/freebsd.o
OBJS += src/unix/kqueue.o
endif

ifeq (netbsd,$(PLATFORM))
LDFLAGS+=-lkvm
OBJS += src/unix/netbsd.o
OBJS += src/unix/kqueue.o
endif

ifeq (openbsd,$(PLATFORM))
LDFLAGS+=-lkvm
OBJS += src/unix/openbsd.o
OBJS += src/unix/kqueue.o
endif

ifneq (,$(findstring cygwin,$(PLATFORM)))
# We drop the --std=c89, it hides CLOCK_MONOTONIC on cygwin
CSTDFLAG = -D_GNU_SOURCE
LDFLAGS+=
OBJS += src/unix/cygwin.o
endif

ifeq (sunos,$(PLATFORM))
RUNNER_LDFLAGS += -pthreads
else
RUNNER_LDFLAGS += -pthread
endif

ifeq ($(HAVE_DTRACE), 1)
DTRACE_HEADER = src/unix/uv-dtrace.h
CPPFLAGS += -Isrc/unix
CFLAGS += -DHAVE_DTRACE
endif

ifneq (darwin,$(PLATFORM))
# Must correspond with UV_VERSION_MAJOR and UV_VERSION_MINOR in src/version.c
SO_LDFLAGS = -Wl,-soname,libuv.so.0.10
endif

RUNNER_LDFLAGS += $(LDFLAGS)

all:
	# Force a sequential build of the static and the shared library.
	# Works around a make quirk where it forgets to (re)build either
	# the *.o or *.pic.o files, depending on what target comes first.
	$(MAKE) -f $(SRCDIR)/Makefile libuv.a
	$(MAKE) -f $(SRCDIR)/Makefile libuv.$(SOEXT)

libuv.a: $(OBJS)
	$(AR) rcs $@ $^

libuv.$(SOEXT):	override CFLAGS += -fPIC
libuv.$(SOEXT):	$(OBJS:%.o=%.pic.o)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(SO_LDFLAGS)

include/uv-private/uv-unix.h: \
	include/uv-private/uv-bsd.h \
	include/uv-private/uv-darwin.h \
	include/uv-private/uv-linux.h \
	include/uv-private/uv-sunos.h

src/unix/internal.h: src/unix/linux-syscalls.h

src/.buildstamp src/unix/.buildstamp test/.buildstamp:
	mkdir -p $(@D)
	touch $@

src/unix/%.o src/unix/%.pic.o: src/unix/%.c include/uv.h include/uv-private/uv-unix.h src/unix/internal.h src/unix/.buildstamp $(DTRACE_HEADER)
	$(CC) $(CSTDFLAG) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

src/%.o src/%.pic.o: src/%.c include/uv.h include/uv-private/uv-unix.h src/.buildstamp
	$(CC) $(CSTDFLAG) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test/%.o: test/%.c include/uv.h test/.buildstamp
	$(CC) $(CSTDFLAG) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean-platform:
	$(RM) test/run-{tests,benchmarks}.dSYM $(OBJS) $(OBJS:%.o=%.pic.o) src/unix/uv-dtrace.h

src/unix/uv-dtrace.h: src/unix/uv-dtrace.d
	dtrace -h -xnolibs -s $< -o $@

src/unix/dtrace.o: src/unix/uv-dtrace.d $(DTRACE_OBJS)
	dtrace -G -s $^ -o $@

src/unix/dtrace.pic.o: src/unix/uv-dtrace.d $(DTRACE_OBJS:%.o=%.pic.o)
	dtrace -G -s $^ -o $@
