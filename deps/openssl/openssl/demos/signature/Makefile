#
# To run the demos when linked with a shared library (default) ensure
# that libcrypto is on the library path. For example:
#
#    LD_LIBRARY_PATH=../.. ./EVP_EC_Signature_demo

TESTS = EVP_EC_Signature_demo \
        EVP_DSA_Signature_demo \
        EVP_ED_Signature_demo \
        rsa_pss_direct \
        rsa_pss_hash

CFLAGS  = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS  = -lcrypto

all: $(TESTS)

EVP_EC_Signature_demo: EVP_EC_Signature_demo.o
EVP_DSA_Signature_demo: EVP_DSA_Signature_demo.o
EVP_ED_Signature_demo: EVP_ED_Signature_demo.o
rsa_pss_direct: rsa_pss_direct.o
rsa_pss_hash: rsa_pss_hash.o

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) *.o $(TESTS)

.PHONY: test
test: all
	@echo "\nSignature tests:"
	@set -e; for tst in $(TESTS); do \
		echo "\n"$$tst; \
		LD_LIBRARY_PATH=../.. ./$$tst; \
	done
