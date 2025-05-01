#
# To run the demos when linked with a shared library (default) ensure that
# libcrypto is on the library path. For example:
#
#    LD_LIBRARY_PATH=../.. ./EVP_PKEY_EC_keygen

TESTS = EVP_PKEY_EC_keygen \
        EVP_PKEY_RSA_keygen \
        EVP_PKEY_DSA_keygen \
        EVP_PKEY_DSA_paramgen \
        EVP_PKEY_DSA_paramvalidate \
        EVP_PKEY_DSA_paramfromdata

CFLAGS  = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS  = -lcrypto

all: $(TESTS)

EVP_PKEY_DSA_keygen.o: EVP_PKEY_DSA_keygen.c dsa.inc
EVP_PKEY_DSA_paramgen.o: EVP_PKEY_DSA_paramgen.c dsa.inc
EVP_PKEY_DSA_paramvalidate.o: EVP_PKEY_DSA_paramvalidate.c dsa.inc
EVP_PKEY_DSA_paramfromdata.o: EVP_PKEY_DSA_paramfromdata.c dsa.inc

EVP_PKEY_EC_keygen: EVP_PKEY_EC_keygen.o
EVP_PKEY_RSA_keygen: EVP_PKEY_RSA_keygen.o
EVP_PKEY_DSA_keygen: EVP_PKEY_DSA_keygen.o
EVP_PKEY_DSA_paramgen: EVP_PKEY_DSA_paramgen.o
EVP_PKEY_DSA_paramvalidate: EVP_PKEY_DSA_paramvalidate.o
EVP_PKEY_DSA_paramfromdata: EVP_PKEY_DSA_paramfromdata.o

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) *.o $(TESTS)

.PHONY: test
test: all
	@echo "\nPKEY tests:"
	@set -e; for tst in $(TESTS); do \
		echo "\n"$$tst; \
		LD_LIBRARY_PATH=../.. ./$$tst; \
	done
