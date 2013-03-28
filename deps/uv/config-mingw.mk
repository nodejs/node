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

CFLAGS=$(CPPFLAGS) -g --std=gnu89 -D_WIN32_WINNT=0x0600
LDFLAGS=-lm

WIN_SRCS=$(wildcard $(SRCDIR)/src/win/*.c)
WIN_OBJS=$(WIN_SRCS:.c=.o)

RUNNER_CFLAGS=$(CFLAGS) -D_GNU_SOURCE # Need _GNU_SOURCE for strdup?
RUNNER_LDFLAGS=$(LDFLAGS)
RUNNER_LIBS=-lws2_32 -lpsapi -liphlpapi
RUNNER_SRC=test/runner-win.c

libuv.a: $(WIN_OBJS) src/fs-poll.o src/inet.o src/uv-common.o src/version.o
	$(AR) rcs $@ $^

src/%.o: src/%.c include/uv.h include/uv-private/uv-win.h
	$(CC) $(CFLAGS) -c $< -o $@

src/win/%.o: src/win/%.c include/uv.h include/uv-private/uv-win.h src/win/internal.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean-platform:
	-rm -f src/win/*.o
