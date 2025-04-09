#
# To run the demos when linked with a shared library (default) ensure that
# libcrypto is on the library path.  For example:
#
#    LD_LIBRARY_PATH=../.. ./aesccm

TESTS = aesccm \
        aesgcm \
        aeskeywrap \
        ariacbc

CFLAGS  = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS  = -lcrypto

all: $(TESTS)

aesccm: aesccm.o
aesgcm: aesgcm.o
aeskeywrap: aeskeywrap.o
ariacbc: ariacbc.o

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) $(TESTS) *.o

.PHONY: test
test: all
	@echo "\nCipher tests:"
	@set -e; for tst in $(TESTS); do \
		echo "\n"$$tst; \
		LD_LIBRARY_PATH=../.. ./$$tst; \
	done
