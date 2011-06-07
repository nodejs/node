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
CFLAGS=--std=gnu89 -pedantic -Wno-variadic-macros -g -DEV_MULTIPLICITY=0
LINKFLAGS=-lm

ifeq (SunOS,$(uname_S))
LINKFLAGS+=-lsocket -lnsl
endif

# Need _GNU_SOURCE for strdup?
RUNNER_CFLAGS=$(CFLAGS) -D_GNU_SOURCE

RUNNER_LINKFLAGS=$(LINKFLAGS) -pthread
RUNNER_LIBS=
RUNNER_SRC=test/runner-unix.c

uv.a: uv-unix.o uv-common.o ev/ev.o
	$(AR) rcs uv.a uv-unix.o uv-common.o ev/ev.o

uv-unix.o: uv-unix.c uv.h uv-unix.h
	$(CC) $(CFLAGS) -c uv-unix.c -o uv-unix.o

uv-common.o: uv-common.c uv.h uv-unix.h
	$(CC) $(CFLAGS) -c uv-common.c -o uv-common.o

ev/ev.o: ev/config.h ev/ev.c
	$(MAKE) -C ev

ev/config.h:
	cd ev && CPPFLAGS=-DEV_MULTIPLICITY=0 ./configure

clean-platform:
	$(MAKE) -C ev clean

distclean-platform:
	$(MAKE) -C ev distclean
