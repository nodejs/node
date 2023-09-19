# Quick instruction:
# To build against an OpenSSL built in the source tree, do this:
#
#    make OPENSSL_INCS_LOCATION=-I../../include OPENSSL_LIBS_LOCATION=-L../..
#
# To run the demos when linked with a shared library (default):
#
#    LD_LIBRARY_PATH=../.. ./aesccm
#    LD_LIBRARY_PATH=../.. ./aesgcm
#    LD_LIBRARY_PATH=../.. ./aeskeywrap
#    LD_LIBRARY_PATH=../.. ./ariacbc

CFLAGS = $(OPENSSL_INCS_LOCATION)
LDFLAGS = $(OPENSSL_LIBS_LOCATION) -lssl -lcrypto

all: aesccm aesgcm aeskeywrap ariacbc

aesccm: aesccm.o
aesgcm: aesgcm.o
aeskeywrap: aeskeywrap.o
ariacbc: ariacbc.o

aesccm aesgcm aeskeywrap ariacbc:
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	$(RM) aesccm aesgcm aeskeywrap ariacbc *.o
