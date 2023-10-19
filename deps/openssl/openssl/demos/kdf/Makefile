#
# To run the demos when linked with a shared library (default):
#
#    LD_LIBRARY_PATH=../.. ./hkdf

CFLAGS = -I../../include -g
LDFLAGS = -L../..
LDLIBS = -lcrypto

all: hkdf pbkdf2 scrypt

%.o: %.c
	$(CC) $(CFLAGS) -c $<

hkdf: hkdf.o
pbkdf2: pbkdf2.o
scrypt: scrypt.o

test: ;

clean:
	$(RM) *.o hkdf pbkdf2 scrypt
