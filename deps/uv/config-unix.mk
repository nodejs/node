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

ifeq (SunOS,$(uname_S))
LINKFLAGS+=-lsocket -lnsl
UV_OS_FILE=uv-sunos.c
endif

ifeq (Darwin,$(uname_S))
LINKFLAGS+=-framework CoreServices
UV_OS_FILE=uv-darwin.c
endif

ifeq (Linux,$(uname_S))
LINKFLAGS+=-lrt
UV_OS_FILE=uv-linux.c
endif

ifeq (FreeBSD,$(uname_S))
LINKFLAGS+=
UV_OS_FILE=uv-freebsd.c
endif

# Need _GNU_SOURCE for strdup?
RUNNER_CFLAGS=$(CFLAGS) -D_GNU_SOURCE

RUNNER_LINKFLAGS=$(LINKFLAGS) -pthread
RUNNER_LIBS=
RUNNER_SRC=test/runner-unix.c

uv.a: uv-unix.o uv-common.o uv-platform.o ev/ev.o c-ares/ares_query.o
	$(AR) rcs uv.a uv-unix.o uv-platform.o uv-common.o ev/ev.o c-ares/*.o

uv-platform.o: $(UV_OS_FILE) uv.h uv-unix.h
	$(CC) $(CFLAGS) -c $(UV_OS_FILE) -o uv-platform.o

uv-unix.o: uv-unix.c uv.h uv-unix.h
	$(CC) $(CFLAGS) -c uv-unix.c -o uv-unix.o

uv-common.o: uv-common.c uv.h uv-unix.h
	$(CC) $(CFLAGS) -c uv-common.c -o uv-common.o

ev/ev.o: ev/config.h ev/ev.c
	$(MAKE) -C ev

ev/config.h:
	cd ev && ./configure

c-ares/Makefile:
	cd c-ares && ./configure

# Really we want to include all of the c-ares .o files in our uv.a static
# library but let's just choose one as a dependency.
c-ares/ares_query.o: c-ares/Makefile
	$(MAKE) -C c-ares

clean-platform:
	$(MAKE) -C ev clean
	$(MAKE) -C c-ares clean

distclean-platform:
	$(MAKE) -C ev distclean
	$(MAKE) -C c-ares clean
