#
# To run the demos when linked with a shared library (default):
#
#    LD_LIBRARY_PATH=../.. ./EVP_PKEY_EC_keygen

CFLAGS = -I../../include -g
LDFLAGS = -L../..
LDLIBS = -lcrypto

all: EVP_PKEY_EC_keygen

%.o: %.c
	$(CC) $(CFLAGS) -c $<

EVP_PKEY_EC_keygen: EVP_PKEY_EC_keygen.o

test: ;

clean:
	$(RM) *.o EVP_PKEY_EC_keygen
