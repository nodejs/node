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

all: quic-server-ssl-poll-http

quic-server-ssl-poll-http: quic-server-ssl-poll-http.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) server *.o

run: quic-server-ssl-poll-http
	LD_LIBRARY_PATH=../../.. ./quic-server-ssl-poll-http 4444 \
	    ../../../test/certs/servercert.pem \
	    ../../../test/certs/serverkey.pem
