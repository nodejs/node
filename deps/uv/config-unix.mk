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
CFLAGS=--std=gnu89 -Wno-variadic-macros -g
LINKFLAGS=-lm

CPPFLAGS += -D_LARGEFILE_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64

ifeq (SunOS,$(uname_S))
EV_CONFIG=config_sunos.h
EIO_CONFIG=config_sunos.h
CPPFLAGS += -Ic-ares/config_sunos
LINKFLAGS+=-lsocket -lnsl
UV_OS_FILE=uv-sunos.c
endif

ifeq (Darwin,$(uname_S))
EV_CONFIG=config_darwin.h
EIO_CONFIG=config_darwin.h
CPPFLAGS += -Ic-ares/config_darwin
LINKFLAGS+=-framework CoreServices
UV_OS_FILE=uv-darwin.c
endif

ifeq (Linux,$(uname_S))
EV_CONFIG=config_linux.h
EIO_CONFIG=config_linux.h
CPPFLAGS += -Ic-ares/config_linux
LINKFLAGS+=-lrt
UV_OS_FILE=uv-linux.c
endif

ifeq (FreeBSD,$(uname_S))
EV_CONFIG=config_freebsd.h
EIO_CONFIG=config_freebsd.h
CPPFLAGS += -Ic-ares/config_freebsd
LINKFLAGS+=
UV_OS_FILE=uv-freebsd.c
endif

ifneq (,$(findstring CYGWIN,$(uname_S)))
EV_CONFIG=config_cygwin.h
EIO_CONFIG=config_cygwin.h
CPPFLAGS += -Ic-ares/config_cygwin
LINKFLAGS+=
UV_OS_FILE=uv-cygwin.c
endif

# Need _GNU_SOURCE for strdup?
RUNNER_CFLAGS=$(CFLAGS) -D_GNU_SOURCE

RUNNER_LINKFLAGS=$(LINKFLAGS) -pthread
RUNNER_LIBS=
RUNNER_SRC=test/runner-unix.c

uv.a: uv-unix.o uv-common.o uv-platform.o ev/ev.o uv-eio.o eio/eio.o $(CARES_OBJS)
	$(AR) rcs uv.a uv-unix.o uv-platform.o uv-common.o uv-eio.o ev/ev.o \
		eio/eio.o $(CARES_OBJS)

uv-platform.o: $(UV_OS_FILE) uv.h uv-unix.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(UV_OS_FILE) -o uv-platform.o

uv-unix.o: uv-unix.c uv.h uv-unix.h
	$(CC) $(CPPFLAGS) -Ieio $(CFLAGS) -c uv-unix.c -o uv-unix.o

uv-common.o: uv-common.c uv.h uv-unix.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c uv-common.c -o uv-common.o

ev/ev.o: ev/ev.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c ev/ev.c -o ev/ev.o -DEV_CONFIG_H=\"$(EV_CONFIG)\"


EIO_CPPFLAGS += $(CPPFLAGS)
EIO_CPPFLAGS += -DEIO_CONFIG_H=\"$(EIO_CONFIG)\"
EIO_CPPFLAGS += -DEIO_STACKSIZE=65536
EIO_CPPFLAGS += -D_GNU_SOURCE

eio/eio.o: eio/eio.c
	$(CC) $(EIO_CPPFLAGS) $(CFLAGS) -c eio/eio.c -o eio/eio.o

uv-eio.o: uv-eio.c
	$(CC) $(CPPFLAGS) -Ieio/ $(CFLAGS) -c uv-eio.c -o uv-eio.o


clean-platform:
	-rm -f c-ares/*.o
	-rm -f ev/*.o
	-rm -f eio/*.o
	-rm -rf test/run-tests.dSYM run-benchmarks.dSYM

distclean-platform:
	-rm -f c-ares/*.o
	-rm -f ev/*.o
	-rm -f eio/*.o
	-rm -rf test/run-tests.dSYM run-benchmarks.dSYM
