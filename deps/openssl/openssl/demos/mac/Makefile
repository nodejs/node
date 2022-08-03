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

all: gmac poly1305

gmac: gmac.o
poly1305: poly1305.o

gmac poly1305:
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	$(RM) gmac poly1305 *.o
