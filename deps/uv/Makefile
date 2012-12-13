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

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

ifdef MSVC
uname_S := MINGW
endif

CPPFLAGS += -Iinclude -Iinclude/uv-private

ifeq (Darwin,$(uname_S))
SOEXT = dylib
else
SOEXT = so
endif

ifneq (,$(findstring MINGW,$(uname_S)))
include config-mingw.mk
else
include config-unix.mk
endif

TESTS=test/blackhole-server.c test/echo-server.c test/test-*.c
BENCHMARKS=test/blackhole-server.c test/echo-server.c test/dns-server.c test/benchmark-*.c

all: libuv.a

test/run-tests$(E): test/run-tests.c test/runner.c $(RUNNER_SRC) $(TESTS) libuv.$(SOEXT)
	$(CC) $(CPPFLAGS) $(RUNNER_CFLAGS) -o $@ $^ $(RUNNER_LIBS) $(RUNNER_LDFLAGS)

test/run-benchmarks$(E): test/run-benchmarks.c test/runner.c $(RUNNER_SRC) $(BENCHMARKS) libuv.$(SOEXT)
	$(CC) $(CPPFLAGS) $(RUNNER_CFLAGS) -o $@ $^ $(RUNNER_LIBS) $(RUNNER_LDFLAGS)

test/echo.o: test/echo.c test/echo.h


.PHONY: clean clean-platform distclean distclean-platform test bench


test: test/run-tests$(E)
	$<

bench: test/run-benchmarks$(E)
	$<

clean: clean-platform
	$(RM) -f src/*.o *.a test/run-tests$(E) test/run-benchmarks$(E)

distclean: distclean-platform
	$(RM) -f src/*.o *.a test/run-tests$(E) test/run-benchmarks$(E)
