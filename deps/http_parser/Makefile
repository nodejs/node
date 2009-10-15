#OPT=-O0 -g -Wall -Wextra -Werror
OPT=-O2

test: http_parser.o test.c
	gcc $(OPT) http_parser.o test.c -o $@

http_parser.o: http_parser.c http_parser.h Makefile
	gcc $(OPT) -c http_parser.c

http_parser.c: http_parser.rl Makefile
	ragel -s -G2 http_parser.rl -o $@

tags: http_parser.rl http_parser.h test.c
	ctags $^

clean:
	rm -f *.o http_parser.c test http_parser.tar

package: http_parser.c
	@rm -rf /tmp/http_parser && mkdir /tmp/http_parser && \
	cp LICENSE README.md Makefile http_parser.c http_parser.rl \
		http_parser.h test.c /tmp/http_parser && \
	cd /tmp && \
	tar -cf http_parser.tar http_parser/
	@echo /tmp/http_parser.tar

.PHONY: clean package
