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

# Use make -f Makefile.gcc PREFIX=i686-w64-mingw32-
# for cross compilation
CC = $(PREFIX)gcc
AR = $(PREFIX)ar
E=.exe

CFLAGS=-g --std=gnu89 -pedantic -Wno-variadic-macros
LINKFLAGS=-lm

RUNNER_CFLAGS=$(CFLAGS) -D_GNU_SOURCE # Need _GNU_SOURCE for strdup?
RUNNER_LINKFLAGS=$(LINKFLAGS)
RUNNER_LIBS=-lws2_32
RUNNER_SRC=test/runner-win.c

uv.a: uv-win.o uv-common.o
	$(AR) rcs uv.a uv-win.o uv-common.o

uv-win.o: uv-win.c uv.h uv-win.h
	$(CC) $(CFLAGS) -c uv-win.c -o uv-win.o

uv-common.o: uv-common.c uv.h uv-win.h
	$(CC) $(CFLAGS) -c uv-common.c -o uv-common.o

distclean-platform:
clean-platform:
