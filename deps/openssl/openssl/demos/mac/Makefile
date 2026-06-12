#
# To run the demos when linked with a shared library (default) ensure
# that libcrypto is on the library path. For example:
#
#    LD_LIBRARY_PATH=../.. ./gmac

TESTS = gmac \
        hmac-sha512 \
        cmac-aes256 \
        poly1305

CFLAGS  = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS  = -lcrypto

all: $(TESTS)

gmac: gmac.o
hmac-sha512: hmac-sha512.o
cmac-aes256: cmac-aes256.o
poly1305: poly1305.o

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) *.o $(TESTS)

.PHONY: test
test: all
	@echo "\nMAC tests:"
	@set -e; for tst in $(TESTS); do \
		echo "\n"$$tst; \
		LD_LIBRARY_PATH=../.. ./$$tst; \
	done
