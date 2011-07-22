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

CC = $(PREFIX)gcc
AR = $(PREFIX)ar
E=
CSTDFLAG=--std=c89 -pedantic -Wall -Wextra -Wno-unused-parameter
CFLAGS=-g
CPPFLAGS += -Isrc/ev
LINKFLAGS=-lm

CPPFLAGS += -D_LARGEFILE_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64

ifeq (SunOS,$(uname_S))
EV_CONFIG=config_sunos.h
EIO_CONFIG=config_sunos.h
CPPFLAGS += -Isrc/ares/config_sunos
LINKFLAGS+=-lsocket -lnsl
UV_OS_FILE=uv-sunos.c
endif

ifeq (Darwin,$(uname_S))
EV_CONFIG=config_darwin.h
EIO_CONFIG=config_darwin.h
CPPFLAGS += -Isrc/ares/config_darwin
LINKFLAGS+=-framework CoreServices
UV_OS_FILE=uv-darwin.c
endif

ifeq (Linux,$(uname_S))
EV_CONFIG=config_linux.h
EIO_CONFIG=config_linux.h
CSTDFLAG += -D_XOPEN_SOURCE=600
CPPFLAGS += -Isrc/ares/config_linux
LINKFLAGS+=-lrt
UV_OS_FILE=uv-linux.c
endif

ifeq (FreeBSD,$(uname_S))
EV_CONFIG=config_freebsd.h
EIO_CONFIG=config_freebsd.h
CPPFLAGS += -Isrc/ares/config_freebsd
LINKFLAGS+=
UV_OS_FILE=uv-freebsd.c
endif

ifneq (,$(findstring CYGWIN,$(uname_S)))
EV_CONFIG=config_cygwin.h
EIO_CONFIG=config_cygwin.h
# We drop the --std=c89, it hides CLOCK_MONOTONIC on cygwin
CSTDFLAG = -D_GNU_SOURCE
CPPFLAGS += -Isrc/ares/config_cygwin
LINKFLAGS+=
UV_OS_FILE=uv-cygwin.c
endif

# Need _GNU_SOURCE for strdup?
RUNNER_CFLAGS=$(CFLAGS) -D_GNU_SOURCE

RUNNER_LINKFLAGS=$(LINKFLAGS) -pthread
RUNNER_LIBS=
RUNNER_SRC=test/runner-unix.c

uv.a: src/uv-unix.o src/uv-common.o src/uv-platform.o src/ev/ev.o src/uv-eio.o src/eio/eio.o $(CARES_OBJS)
	$(AR) rcs uv.a src/uv-unix.o src/uv-platform.o src/uv-common.o src/uv-eio.o src/ev/ev.o \
		src/eio/eio.o $(CARES_OBJS)

src/uv-platform.o: src/$(UV_OS_FILE) include/uv.h include/uv-unix.h
	$(CC) $(CSTDFLAG) $(CPPFLAGS) $(CFLAGS) -c src/$(UV_OS_FILE) -o src/uv-platform.o

src/uv-unix.o: src/uv-unix.c include/uv.h include/uv-unix.h
	$(CC) $(CSTDFLAG) $(CPPFLAGS) -Ieio $(CFLAGS) -c src/uv-unix.c -o src/uv-unix.o

src/uv-common.o: src/uv-common.c include/uv.h include/uv-unix.h
	$(CC) $(CSTDFLAG) $(CPPFLAGS) $(CFLAGS) -c src/uv-common.c -o src/uv-common.o

src/ev/ev.o: src/ev/ev.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/ev/ev.c -o src/ev/ev.o -DEV_CONFIG_H=\"$(EV_CONFIG)\"


EIO_CPPFLAGS += $(CPPFLAGS)
EIO_CPPFLAGS += -DEIO_CONFIG_H=\"$(EIO_CONFIG)\"
EIO_CPPFLAGS += -DEIO_STACKSIZE=65536
EIO_CPPFLAGS += -D_GNU_SOURCE

src/eio/eio.o: src/eio/eio.c
	$(CC) $(EIO_CPPFLAGS) $(CFLAGS) -c src/eio/eio.c -o src/eio/eio.o

src/uv-eio.o: src/uv-eio.c
	$(CC) $(CPPFLAGS) -Isrc/eio/ $(CSTDFLAG) $(CFLAGS) -c src/uv-eio.c -o src/uv-eio.o


clean-platform:
	-rm -f src/ares/*.o
	-rm -f src/ev/*.o
	-rm -f src/eio/*.o
	-rm -rf test/run-tests.dSYM run-benchmarks.dSYM

distclean-platform:
	-rm -f src/ares/*.o
	-rm -f src/ev/*.o
	-rm -f src/eio/*.o
	-rm -rf test/run-tests.dSYM run-benchmarks.dSYM
