#
# To run the demos when linked with a shared library (default):
#
#    LD_LIBRARY_PATH=../.. ./EVP_Signature_demo

CFLAGS = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS = -lcrypto

all: EVP_Signature_demo rsa_pss_direct rsa_pss_hash

%.o: %.c
	$(CC) $(CFLAGS) -c $<

EVP_Signature_demo: EVP_Signature_demo.o
rsa_pss_direct: rsa_pss_direct.o
rsa_pss_hash: rsa_pss_hash.o

test: ;

clean:
	$(RM) *.o EVP_Signature_demo rsa_pss_direct rsa_pss_hash
