#
# To run the demos when linked with a shared library (default) ensure
# that libcrypto is on the library path. For example:
#
#    LD_LIBRARY_PATH=../.. ./x25519

TESTS = x25519 ecdh

CFLAGS  = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS  = -lcrypto

all: $(TESTS)

x25519: x25519.o
ecdh: ecdh.o

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) *.o $(TESTS)

.PHONY: test
test: all
	@echo "\nKeyExchange tests:"
	@set -e; for tst in $(TESTS); do \
		echo "\n"$$tst; \
		LD_LIBRARY_PATH=../.. ./$$tst; \
	done
