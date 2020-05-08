CFLAGS	= -I${OPENSSL}/include -fPIC -Wall -Werror -g
LDFLAGS	= -L${OPENSSL}/lib -lssl -lcrypto

OPENSSL = /usr
PREFIX  = /usr
LIB	= danessl
PROG1	= connected
PROG2 	= offline
OBJS	= danessl.o
SHLIB_EXT = .so
SHLIB	= lib${LIB}${SHLIB_EXT}
SHLIB_LDFLAGS = -shared

all: ${SHLIB} ${PROG1} ${PROG2}

${SHLIB}: ${OBJS}
	$(CC) ${SHLIB_LDFLAGS} -o $@ ${OBJS} ${LDFLAGS}

${PROG1}: ${PROG1}.o ${OBJS}
	$(CC) -o $@ ${PROG1}.o -L. -l${LIB} ${LDFLAGS}

${PROG2}: ${PROG2}.o ${OBJS}
	$(CC) -o $@ ${PROG2}.o -L. -l${LIB} ${LDFLAGS}

clean:
	rm -f ${SHLIB} ${PROG1} ${PROG2} *.o

install:
	cp danessl.h ${PREFIX}/include/
	cp ${SHLIB} ${PREFIX}/lib/
