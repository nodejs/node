CC:=gcc
AR:=ar
CFLAGS:=-Wall -Wextra -Wno-unused-parameter -O2 -g

.PHONY: clean all test

all: libyxml.a

yxml.c: yxml.c.in yxml-gen.pl yxml-states
	perl yxml-gen.pl

libyxml.a: yxml.c yxml.h
	$(CC) $(CFLAGS) -I. -c yxml.c
	$(AR) rcs libyxml.a yxml.o

test/test: libyxml.a test/test.c
	$(CC) $(CFLAGS) -I. test/test.c libyxml.a -o test/test

test: test/test
	cd test && sh test.sh

# yxml.c isn't cleaned, since it's included in git
clean:
	rm -f yxml.o libyxml.a test/*.test test/test
