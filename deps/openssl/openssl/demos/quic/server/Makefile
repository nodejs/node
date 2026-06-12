#
# To run the demo when linked with a shared library (default) ensure that
# libcrypto and libssl are on the library path. For example:
#
#    LD_LIBRARY_PATH=../../.. ./server 4444 \
#    	../../../test/certs/servercert.pem \
#    	../../../test/certs/serverkey.pem
#
CFLAGS  += -I../../../include -g -Wall -Wsign-compare
LDFLAGS += -L../../..
LDLIBS  = -lcrypto -lssl

.PHONY: all server clean run s_client

all: server

server: server.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) server *.o

run: server
	LD_LIBRARY_PATH=../../.. ./server 4444 \
	    ../../../test/certs/servercert.pem \
	    ../../../test/certs/serverkey.pem

s_client:
	LD_LIBRARY_PATH=../../.. ../../../apps/openssl \
	    s_client -quic -quiet -alpn ossltest -connect 127.0.0.1:4444 || true
