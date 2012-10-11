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

ifneq (,$(findstring MINGW,$(uname_S)))
include config-mingw.mk
else
include config-unix.mk
endif

TESTS=test/blackhole-server.c test/echo-server.c test/test-*.c
BENCHMARKS=test/blackhole-server.c test/echo-server.c test/dns-server.c test/benchmark-*.c

all: libuv.a

test/run-tests$(E): test/*.h test/run-tests.c $(RUNNER_SRC) test/runner-unix.c $(TESTS) libuv.a
	$(CC) $(CPPFLAGS) $(RUNNER_CFLAGS) -o test/run-tests test/run-tests.c \
		test/runner.c $(RUNNER_SRC) $(TESTS) libuv.a $(RUNNER_LIBS) $(RUNNER_LINKFLAGS)

test/run-benchmarks$(E): test/*.h test/run-benchmarks.c test/runner.c $(RUNNER_SRC) $(BENCHMARKS) libuv.a
	$(CC) $(CPPFLAGS) $(RUNNER_CFLAGS) -o test/run-benchmarks test/run-benchmarks.c \
		 test/runner.c $(RUNNER_SRC) $(BENCHMARKS) libuv.a $(RUNNER_LIBS) $(RUNNER_LINKFLAGS)

test/echo.o: test/echo.c test/echo.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c test/echo.c -o test/echo.o


.PHONY: clean clean-platform distclean distclean-platform test bench


test: test/run-tests$(E)
	test/run-tests

#test-%:	test/run-tests$(E)
#	test/run-tests $(@:test-%=%)

bench: test/run-benchmarks$(E)
	test/run-benchmarks

#bench-%:	test/run-benchmarks$(E)
#	test/run-benchmarks $(@:bench-%=%)

clean: clean-platform
	$(RM) -f src/*.o *.a test/run-tests$(E) test/run-benchmarks$(E)

distclean: distclean-platform
	$(RM) -f src/*.o *.a test/run-tests$(E) test/run-benchmarks$(E)
