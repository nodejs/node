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
CPPFLAGS += -Isrc
LDFLAGS=-lm

CPPFLAGS += -D_LARGEFILE_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64

RUNNER_SRC=test/runner-unix.c
RUNNER_CFLAGS=$(CFLAGS) -Itest
RUNNER_LDFLAGS=-L"$(PWD)" -luv -Xlinker -rpath -Xlinker "$(PWD)"

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

ifeq (SunOS,$(uname_S))
CPPFLAGS += -D__EXTENSIONS__ -D_XOPEN_SOURCE=500
LDFLAGS+=-lkstat -lnsl -lsendfile -lsocket
# Library dependencies are not transitive.
RUNNER_LDFLAGS += $(LDFLAGS)
OBJS += src/unix/sunos.o
endif

ifeq (AIX,$(uname_S))
CPPFLAGS += -Isrc/ares/config_aix -D_ALL_SOURCE -D_XOPEN_SOURCE=500
LDFLAGS+= -lperfstat
OBJS += src/unix/aix.o
endif

ifeq (Darwin,$(uname_S))
CPPFLAGS += -D_DARWIN_USE_64_BIT_INODE=1
LDFLAGS+=-framework CoreServices -dynamiclib -install_name "@rpath/libuv.dylib"
SOEXT = dylib
OBJS += src/unix/darwin.o
OBJS += src/unix/kqueue.o
OBJS += src/unix/fsevents.o
endif

ifeq (Linux,$(uname_S))
CSTDFLAG += -D_GNU_SOURCE
LDFLAGS+=-ldl -lrt
RUNNER_CFLAGS += -D_GNU_SOURCE
OBJS += src/unix/linux/linux-core.o \
        src/unix/linux/inotify.o    \
        src/unix/linux/syscalls.o
endif

ifeq (FreeBSD,$(uname_S))
LDFLAGS+=-lkvm
OBJS += src/unix/freebsd.o
OBJS += src/unix/kqueue.o
endif

ifeq (DragonFly,$(uname_S))
LDFLAGS+=-lkvm
OBJS += src/unix/freebsd.o
OBJS += src/unix/kqueue.o
endif

ifeq (NetBSD,$(uname_S))
LDFLAGS+=-lkvm
OBJS += src/unix/netbsd.o
OBJS += src/unix/kqueue.o
endif

ifeq (OpenBSD,$(uname_S))
LDFLAGS+=-lkvm
OBJS += src/unix/openbsd.o
OBJS += src/unix/kqueue.o
endif

ifneq (,$(findstring CYGWIN,$(uname_S)))
# We drop the --std=c89, it hides CLOCK_MONOTONIC on cygwin
CSTDFLAG = -D_GNU_SOURCE
LDFLAGS+=
OBJS += src/unix/cygwin.o
endif

ifeq (SunOS,$(uname_S))
RUNNER_LDFLAGS += -pthreads
else
RUNNER_LDFLAGS += -pthread
endif

libuv.a: $(OBJS)
	$(AR) rcs $@ $^

libuv.$(SOEXT):	override CFLAGS += -fPIC
libuv.$(SOEXT):	$(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c include/uv.h include/uv-private/uv-unix.h
	$(CC) $(CSTDFLAG) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

src/unix/%.o: src/unix/%.c include/uv.h include/uv-private/uv-unix.h src/unix/internal.h
	$(CC) $(CSTDFLAG) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean-platform:
	-rm -f src/unix/*.o
	-rm -f src/unix/linux/*.o
	-rm -rf test/run-tests.dSYM run-benchmarks.dSYM

distclean-platform:
	-rm -f src/unix/*.o
	-rm -f src/unix/linux/*.o
	-rm -rf test/run-tests.dSYM run-benchmarks.dSYM
