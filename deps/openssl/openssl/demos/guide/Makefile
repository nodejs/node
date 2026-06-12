#
# To run the demos when linked with a shared library (default) ensure that
# libcrypto and libssl are on the library path. For example:
#
#    LD_LIBRARY_PATH=../.. ./tls-client-block www.example.com 443

TESTS = tls-client-block \
        tls-server-block \
        quic-client-block \
        quic-multi-stream \
        tls-client-non-block \
        quic-server-block \
        quic-server-non-block \
        quic-client-non-block

CFLAGS  = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS  = -lcrypto -lssl

all: $(TESTS) chain

tls-client-block: tls-client-block.o
tls-server-block: tls-server-block.o
quic-client-block: quic-client-block.o
quic-multi-stream: quic-multi-stream.o
tls-client-non-block: tls-client-non-block.o
quic-server-block: quic-server-block.o
quic-server-non-block: quic-server-non-block.o
quic-client-non-block: quic-client-non-block.o

chain: chain.pem
pkey.pem:
	openssl genpkey -algorithm rsa -out pkey.pem -pkeyopt rsa_keygen_bits:2048
chain.pem: pkey.pem
	openssl req -x509 -new -key pkey.pem -days 36500 -subj '/CN=localhost' -out chain.pem

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) $(TESTS) *.o

.PHONY: test chain
test: all
	@echo "\nTLS and QUIC tests:"
	@echo "skipped"
