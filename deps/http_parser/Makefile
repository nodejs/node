CPPFLAGS?=-Wall -Wextra -Werror -I.
OPT_DEBUG=$(CPPFLAGS) -O0 -g -DHTTP_PARSER_STRICT=1
OPT_FAST=$(CPPFLAGS) -O3 -DHTTP_PARSER_STRICT=0

CC?=gcc
AR?=ar


test: test_g test_fast
	./test_g
	./test_fast

test_g: http_parser_g.o test_g.o
	$(CC) $(OPT_DEBUG) http_parser_g.o test_g.o -o $@

test_g.o: test.c http_parser.h Makefile
	$(CC) $(OPT_DEBUG) -c test.c -o $@

test.o: test.c http_parser.h Makefile
	$(CC) $(OPT_FAST) -c test.c -o $@

http_parser_g.o: http_parser.c http_parser.h Makefile
	$(CC) $(OPT_DEBUG) -c http_parser.c -o $@

test-valgrind: test_g
	valgrind ./test_g

http_parser.o: http_parser.c http_parser.h Makefile
	$(CC) $(OPT_FAST) -c http_parser.c

test_fast: http_parser.o test.c http_parser.h
	$(CC) $(OPT_FAST) http_parser.o test.c -o $@

test-run-timed: test_fast
	while(true) do time ./test_fast > /dev/null; done

package: http_parser.o
	$(AR) rcs libhttp_parser.a http_parser.o

tags: http_parser.c http_parser.h test.c
	ctags $^

clean:
	rm -f *.o *.a test test_fast test_g http_parser.tar tags

.PHONY: clean package test-run test-run-timed test-valgrind
