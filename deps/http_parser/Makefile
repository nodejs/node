OPT_DEBUG=-O0 -g -Wall -Wextra -Werror 
OPT_FAST=-O3 -DHTTP_PARSER_STRICT=0


http_parser_g.o: http_parser.c http_parser.h Makefile
	gcc $(OPT_DEBUG) -c http_parser.c

test_g: http_parser_g.o test.c
	gcc $(OPT_DEBUG) http_parser.o test.c -o $@

test-run: test_g
	./test_g


http_parser.o: http_parser.c http_parser.h Makefile
	gcc $(OPT_FAST) -c http_parser.c

test: http_parser.o test.c
	gcc $(OPT_FAST) http_parser.o test.c -o $@

test-run-timed: test
	while(true) do time ./test > /dev/null; done


tags: http_parser.c http_parser.h test.c
	ctags $^

clean:
	rm -f *.o test test_g http_parser.tar

.PHONY: clean package test-run test-run-timed
