#
# To run the demos when linked with a shared library (default):
#
#    LD_LIBRARY_PATH=../.. ./EVP_Signature_demo

CFLAGS = -I../../include -g
LDFLAGS = -L../..
LDLIBS = -lcrypto

all: EVP_Signature_demo

%.o: %.c
	$(CC) $(CFLAGS) -c $<

EVP_Signature_demo: EVP_Signature_demo.o

test: ;

clean:
	$(RM) *.o EVP_Signature_demo
