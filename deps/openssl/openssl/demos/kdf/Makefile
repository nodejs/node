#
# To run the demos when linked with a shared library (default) ensure that
# libcrypto is on the library path. For example:
#
#    LD_LIBRARY_PATH=../.. ./hkdf

TESTS = hkdf \
        pbkdf2 \
        scrypt \
        argon2

CFLAGS  = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS  = -lcrypto

all: $(TESTS)

hkdf: hkdf.o
pbkdf2: pbkdf2.o
scrypt: scrypt.o
argon2: argon2.o

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) *.o $(TESTS)

.PHONY: test
test: all
	@echo "\nKDF tests:"
	@set -e; for tst in $(TESTS); do \
		echo "\n"$$tst; \
		LD_LIBRARY_PATH=../.. ./$$tst; \
	done
