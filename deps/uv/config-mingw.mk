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

CFLAGS=$(CPPFLAGS) -g --std=gnu89 -D_WIN32_WINNT=0x0501 -Isrc/ares/config_win32
LINKFLAGS=-lm

CARES_OBJS += src/ares/windows_port.o
CARES_OBJS += src/ares/ares_platform.o
WIN_SRCS=$(wildcard src/win/*.c)
WIN_OBJS=$(WIN_SRCS:.c=.o)

RUNNER_CFLAGS=$(CFLAGS) -D_GNU_SOURCE # Need _GNU_SOURCE for strdup?
RUNNER_LINKFLAGS=$(LINKFLAGS)
RUNNER_LIBS=-lws2_32
RUNNER_SRC=test/runner-win.c

uv.a: $(WIN_OBJS) src/uv-common.o $(CARES_OBJS)
	$(AR) rcs uv.a src/win/*.o src/uv-common.o $(CARES_OBJS)

src/win/%.o: src/win/%.c src/win/internal.h
	$(CC) $(CFLAGS) -o $@ -c $<

src/uv-common.o: src/uv-common.c include/uv.h include/uv-private/uv-win.h
	$(CC) $(CFLAGS) -c src/uv-common.c -o src/uv-common.o

EIO_CPPFLAGS += $(CPPFLAGS)
EIO_CPPFLAGS += -DEIO_STACKSIZE=65536
EIO_CPPFLAGS += -D_GNU_SOURCE

clean-platform:
	-rm -f src/ares/*.o
	-rm -f src/eio/*.o
	-rm -f src/win/*.o

distclean-platform:
	-rm -f src/ares/*.o
	-rm -f src/eio/*.o
	-rm -f src/win/*.o
