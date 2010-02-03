OPT_DEBUG=-O0 -g -Wall -Wextra -Werror -I.
OPT_FAST=-O3 -DHTTP_PARSER_STRICT=0 -I.


test: test_debug
	./test_debug

test_debug: http_parser_debug.o test.c
	gcc $(OPT_DEBUG) http_parser.o test.c -o $@

http_parser_debug.o: http_parser.c http_parser.h Makefile
	gcc $(OPT_DEBUG) -c http_parser.c

test-valgrind: test_debug
	valgrind ./test_debug

http_parser.o: http_parser.c http_parser.h Makefile
	gcc $(OPT_FAST) -c http_parser.c

test_fast: http_parser.o test.c
	gcc $(OPT_FAST) http_parser.o test.c -o $@

test-run-timed: test_fast
	while(true) do time ./test_fast > /dev/null; done


tags: http_parser.c http_parser.h test.c
	ctags $^

clean:
	rm -f *.o test test_fast test_debug http_parser.tar tags

.PHONY: clean package test-run test-run-timed test-valgrind
