CC?=gcc
AR?=ar

CPPFLAGS += -I.
CPPFLAGS_DEBUG = $(CPPFLAGS) -DHTTP_PARSER_STRICT=1 -DHTTP_PARSER_DEBUG=1
CPPFLAGS_DEBUG += $(CPPFLAGS_DEBUG_EXTRA)
CPPFLAGS_FAST = $(CPPFLAGS) -DHTTP_PARSER_STRICT=0 -DHTTP_PARSER_DEBUG=0
CPPFLAGS_FAST += $(CPPFLAGS_FAST_EXTRA)

CFLAGS += -Wall -Wextra -Werror
CFLAGS_DEBUG = $(CFLAGS) -O0 -g $(CFLAGS_DEBUG_EXTRA)
CFLAGS_FAST = $(CFLAGS) -O3 $(CFLAGS_FAST_EXTRA)


test: test_g test_fast
	./test_g
	./test_fast

test_g: http_parser_g.o test_g.o
	$(CC) $(CFLAGS_DEBUG) $(LDFLAGS) http_parser_g.o test_g.o -o $@

test_g.o: test.c http_parser.h Makefile
	$(CC) $(CPPFLAGS_DEBUG) $(CFLAGS_DEBUG) -c test.c -o $@

http_parser_g.o: http_parser.c http_parser.h Makefile
	$(CC) $(CPPFLAGS_DEBUG) $(CFLAGS_DEBUG) -c http_parser.c -o $@

test_fast: http_parser.o test.o http_parser.h
	$(CC) $(CFLAGS_FAST) $(LDFLAGS) http_parser.o test.o -o $@

test.o: test.c http_parser.h Makefile
	$(CC) $(CPPFLAGS_FAST) $(CFLAGS_FAST) -c test.c -o $@

http_parser.o: http_parser.c http_parser.h Makefile
	$(CC) $(CPPFLAGS_FAST) $(CFLAGS_FAST) -c http_parser.c

test-run-timed: test_fast
	while(true) do time ./test_fast > /dev/null; done

test-valgrind: test_g
	valgrind ./test_g

package: http_parser.o
	$(AR) rcs libhttp_parser.a http_parser.o

tags: http_parser.c http_parser.h test.c
	ctags $^

clean:
	rm -f *.o *.a test test_fast test_g http_parser.tar tags

.PHONY: clean package test-run test-run-timed test-valgrind
