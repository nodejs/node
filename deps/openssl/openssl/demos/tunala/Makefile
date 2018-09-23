# Edit these to suit
#
# Oh yeah, and please read the README too.


SSL_HOMEDIR=../..
SSL_INCLUDEDIR=$(SSL_HOMEDIR)/include
SSL_LIBDIR=$(SSL_HOMEDIR)

RM=rm -f
CC=gcc
DEBUG_FLAGS=-g -ggdb3 -Wall -Wshadow
INCLUDE_FLAGS=-I$(SSL_INCLUDEDIR)
CFLAGS=$(DEBUG_FLAGS) $(INCLUDE_FLAGS) -DNO_CONFIG_H
COMPILE=$(CC) $(CFLAGS) -c

# Edit, particularly the "-ldl" if not building with "dlfcn" support
LINK_FLAGS=-L$(SSL_LIBDIR) -lssl -lcrypto -ldl

SRCS=buffer.c cb.c ip.c sm.c tunala.c breakage.c
OBJS=buffer.o cb.o ip.o sm.o tunala.o breakage.o

TARGETS=tunala

default: $(TARGETS)

clean:
	$(RM) $(OBJS) $(TARGETS) *.bak core

.c.o:
	$(COMPILE) $<

tunala: $(OBJS)
	$(CC) -o tunala $(OBJS) $(LINK_FLAGS)

# Extra dependencies, should really use makedepend
buffer.o: buffer.c tunala.h
cb.o: cb.c tunala.h
ip.o: ip.c tunala.h
sm.o: sm.c tunala.h
tunala.o: tunala.c tunala.h
