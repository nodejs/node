#
# To run the demo when linked with a shared library (default) ensure that
# libcrypto and libssl are on the library path. For example:
#
#    LD_LIBRARY_PATH=../.. ./ossl-nghttp3-demo www.example.com:443

# when using gcc the below CFLAGS make sure you follow most of the rules of OpenSSL coding style.
#CFLAGS  += -I../../include -g -Wall -Wsign-compare -std=c90 -pedantic -pedantic-errors -Wno-long-long

CFLAGS  += -I../../include -g -Wall -Wsign-compare
LDFLAGS += -L../..
LDLIBS  = -lcrypto -lssl -lnghttp3

all: ossl-nghttp3-demo ossl-nghttp3-demo-server

ossl-nghttp3-demo: ossl-nghttp3-demo.o ossl-nghttp3.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

ossl-nghttp3-demo-server: ossl-nghttp3-demo-server.o ossl-nghttp3.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) ossl-nghttp3-demo ossl-nghttp3-demo-server *.o

.PHONY: test
test: all
	@echo "\nHTTP/3 tests:"
	@echo "skipped"
