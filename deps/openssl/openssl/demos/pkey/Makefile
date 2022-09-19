#
# To run the demos when linked with a shared library (default):
#
#    LD_LIBRARY_PATH=../.. ./EVP_PKEY_EC_keygen
#    LD_LIBRARY_PATH=../.. ./EVP_PKEY_RSA_keygen

CFLAGS = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS = -lcrypto

all: EVP_PKEY_EC_keygen EVP_PKEY_RSA_keygen

%.o: %.c
	$(CC) $(CFLAGS) -c $<

EVP_PKEY_EC_keygen: EVP_PKEY_EC_keygen.o

EVP_PKEY_RSA_keygen: EVP_PKEY_RSA_keygen.o

test: ;

clean:
	$(RM) *.o EVP_PKEY_EC_keygen EVP_PKEY_RSA_keygen
