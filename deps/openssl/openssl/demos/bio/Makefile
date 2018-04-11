# Quick instruction:
# To build against an OpenSSL built in the source tree, do this:
#
#    make OPENSSL_INCS_LOCATION=-I../../include OPENSSL_LIBS_LOCATION=-L../..
#
# To run the demos when linked with a shared library (default):
#
#    LD_LIBRARY_PATH=../.. ./server-arg
#    LD_LIBRARY_PATH=../.. ./server-cmod
#    LD_LIBRARY_PATH=../.. ./server-conf
#    LD_LIBRARY_PATH=../.. ./client-arg
#    LD_LIBRARY_PATH=../.. ./client-conf
#    LD_LIBRARY_PATH=../.. ./saccept
#    LD_LIBRARY_PATH=../.. ./sconnect

CFLAGS = $(OPENSSL_INCS_LOCATION)
LDFLAGS = $(OPENSSL_LIBS_LOCATION) -lssl -lcrypto $(EX_LIBS)

all: client-arg client-conf saccept sconnect server-arg server-cmod server-conf

client-arg: client-arg.o
client-conf: client-conf.o
saccept: saccept.o
sconnect: sconnect.o
server-arg: server-arg.o
server-cmod: server-cmod.o
server-conf: server-conf.o

client-arg client-conf saccept sconnect server-arg server-cmod server-conf:
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
