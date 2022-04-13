#
# To run the demos when linked with a shared library (default):
#
#    LD_LIBRARY_PATH=../.. ./EVP_MD_demo

CFLAGS = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS = -lcrypto

all: EVP_MD_demo EVP_MD_stdin EVP_MD_xof BIO_f_md

%.o: %.c
	$(CC) $(CFLAGS) -c $<

EVP_MD_demo: EVP_MD_demo.o
EVP_MD_stdin: EVP_MD_stdin.o
EVP_MD_xof: EVP_MD_xof.o
BIO_f_md: BIO_f_md.o

test: ;

clean:
	$(RM) *.o EVP_MD_demo EVP_MD_stdin EVP_MD_xof BIO_f_md
