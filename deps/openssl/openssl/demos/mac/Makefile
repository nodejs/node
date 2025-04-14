# Quick instruction:
# To build against an OpenSSL built in the source tree, do this:
#
#    make OPENSSL_INCS_LOCATION=-I../../include OPENSSL_LIBS_LOCATION=-L../..
#
# To run the demos when linked with a shared library (default):
#
#    LD_LIBRARY_PATH=../.. ./gmac
#    LD_LIBRARY_PATH=../.. ./poly1305

CFLAGS = $(OPENSSL_INCS_LOCATION) -Wall
LDFLAGS = $(OPENSSL_LIBS_LOCATION) -lssl -lcrypto

all: gmac hmac-sha512 cmac-aes256 poly1305

gmac: gmac.o
hmac-sha512: hmac-sha512.o
cmac-aes256: cmac-aes256.o
poly1305: poly1305.o

gmac hmac-sha512 cmac-aes256 poly1305:
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	$(RM) gmac hmac-sha512 cmac-aes256 poly1305 *.o
