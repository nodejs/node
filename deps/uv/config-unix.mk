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
CPPFLAGS += -Isrc/unix/ev
LINKFLAGS=-lm

CPPFLAGS += -D_LARGEFILE_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64

OBJS += src/unix/core.o
OBJS += src/unix/dl.o
OBJS += src/unix/fs.o
OBJS += src/unix/cares.o
OBJS += src/unix/udp.o
OBJS += src/unix/error.o
OBJS += src/unix/thread.o
OBJS += src/unix/process.o
OBJS += src/unix/tcp.o
OBJS += src/unix/pipe.o
OBJS += src/unix/tty.o
OBJS += src/unix/stream.o

ifeq (SunOS,$(uname_S))
EV_CONFIG=config_sunos.h
EIO_CONFIG=config_sunos.h
CPPFLAGS += -Isrc/ares/config_sunos -D__EXTENSIONS__ -D_XOPEN_SOURCE=500
LINKFLAGS+=-lsocket -lnsl -lkstat
OBJS += src/unix/sunos.o
endif

ifeq (Darwin,$(uname_S))
EV_CONFIG=config_darwin.h
EIO_CONFIG=config_darwin.h
CPPFLAGS += -Isrc/ares/config_darwin
LINKFLAGS+=-framework CoreServices
OBJS += src/unix/darwin.o
OBJS += src/unix/kqueue.o
endif

ifeq (Linux,$(uname_S))
EV_CONFIG=config_linux.h
EIO_CONFIG=config_linux.h
CSTDFLAG += -D_GNU_SOURCE
CPPFLAGS += -Isrc/ares/config_linux
LINKFLAGS+=-lrt
OBJS += src/unix/linux.o
endif

ifeq (FreeBSD,$(uname_S))
EV_CONFIG=config_freebsd.h
EIO_CONFIG=config_freebsd.h
CPPFLAGS += -Isrc/ares/config_freebsd
LINKFLAGS+=
OBJS += src/unix/freebsd.o
OBJS += src/unix/kqueue.o
endif

ifeq (DragonFly,$(uname_S))
EV_CONFIG=config_freebsd.h
EIO_CONFIG=config_freebsd.h
CPPFLAGS += -Isrc/ares/config_freebsd
LINKFLAGS+=
OBJS += src/unix/freebsd.o
OBJS += src/unix/kqueue.o
endif

ifeq (NetBSD,$(uname_S))
EV_CONFIG=config_netbsd.h
EIO_CONFIG=config_netbsd.h
CPPFLAGS += -Isrc/ares/config_netbsd
LINKFLAGS+=
OBJS += src/unix/netbsd.o
OBJS += src/unix/kqueue.o
endif

ifeq (OpenBSD,$(uname_S))
EV_CONFIG=config_openbsd.h
EIO_CONFIG=config_openbsd.h
CPPFLAGS += -Isrc/ares/config_openbsd
LINKFLAGS+=
OBJS += src/unix/openbsd.o
OBJS += src/unix/kqueue.o
endif

ifneq (,$(findstring CYGWIN,$(uname_S)))
EV_CONFIG=config_cygwin.h
EIO_CONFIG=config_cygwin.h
# We drop the --std=c89, it hides CLOCK_MONOTONIC on cygwin
CSTDFLAG = -D_GNU_SOURCE
CPPFLAGS += -Isrc/ares/config_cygwin
LINKFLAGS+=
OBJS += src/unix/cygwin.o
endif

# Need _GNU_SOURCE for strdup?
RUNNER_CFLAGS=$(CFLAGS) -D_GNU_SOURCE
RUNNER_LINKFLAGS=$(LINKFLAGS)

ifeq (SunOS,$(uname_S))
RUNNER_LINKFLAGS += -pthreads
else
RUNNER_LINKFLAGS += -pthread
endif

RUNNER_LIBS=
RUNNER_SRC=test/runner-unix.c

uv.a: $(OBJS) src/uv-common.o src/unix/ev/ev.o src/unix/uv-eio.o src/unix/eio/eio.o $(CARES_OBJS)
	$(AR) rcs uv.a $(OBJS) src/uv-common.o src/unix/uv-eio.o src/unix/ev/ev.o src/unix/eio/eio.o $(CARES_OBJS)

src/unix/%.o: src/unix/%.c include/uv.h include/uv-private/uv-unix.h src/unix/internal.h
	$(CC) $(CSTDFLAG) $(CPPFLAGS) -Isrc  $(CFLAGS) -c $< -o $@

src/uv-common.o: src/uv-common.c include/uv.h include/uv-private/uv-unix.h
	$(CC) $(CSTDFLAG) $(CPPFLAGS) $(CFLAGS) -c src/uv-common.c -o src/uv-common.o

src/unix/ev/ev.o: src/unix/ev/ev.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/unix/ev/ev.c -o src/unix/ev/ev.o -DEV_CONFIG_H=\"$(EV_CONFIG)\"


EIO_CPPFLAGS += $(CPPFLAGS)
EIO_CPPFLAGS += -DEIO_CONFIG_H=\"$(EIO_CONFIG)\"
EIO_CPPFLAGS += -DEIO_STACKSIZE=262144
EIO_CPPFLAGS += -D_GNU_SOURCE

src/unix/eio/eio.o: src/unix/eio/eio.c
	$(CC) $(EIO_CPPFLAGS) $(CFLAGS) -c src/unix/eio/eio.c -o src/unix/eio/eio.o

src/unix/uv-eio.o: src/unix/uv-eio.c
	$(CC) $(CPPFLAGS) -Isrc/unix/eio/ $(CSTDFLAG) $(CFLAGS) -c src/unix/uv-eio.c -o src/unix/uv-eio.o


clean-platform:
	-rm -f src/ares/*.o
	-rm -f src/unix/ev/*.o
	-rm -f src/unix/eio/*.o
	-rm -f src/unix/*.o
	-rm -rf test/run-tests.dSYM run-benchmarks.dSYM

distclean-platform:
	-rm -f src/ares/*.o
	-rm -f src/unix/ev/*.o
	-rm -f src/unix/*.o
	-rm -f src/unix/eio/*.o
	-rm -rf test/run-tests.dSYM run-benchmarks.dSYM
