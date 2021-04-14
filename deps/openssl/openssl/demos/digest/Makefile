#
# To run the demos when linked with a shared library (default):
#
#    LD_LIBRARY_PATH=../.. ./EVP_MD_demo

CFLAGS = -I../../include -g
LDFLAGS = -L../..
LDLIBS = -lcrypto

all: EVP_MD_demo EVP_MD_stdin BIO_f_md

%.o: %.c
	$(CC) $(CFLAGS) -c $<

EVP_MD_demo: EVP_MD_demo.o
EVP_MD_stdin: EVP_MD_stdin.o
BIO_f_md: BIO_f_md.o

test: ;

clean:
	$(RM) *.o EVP_MD_demo EVP_MD_stdin BIO_f_md
