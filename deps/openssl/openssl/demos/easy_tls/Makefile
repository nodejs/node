# Makefile for easy-tls example application (rudimentary client and server)
# $Id: Makefile,v 1.2 2001/09/18 09:15:40 bodo Exp $

SOLARIS_CFLAGS=-Wall -pedantic -g -O2
SOLARIS_LIBS=-lxnet

LINUX_CFLAGS=-Wall -pedantic -g -O2
LINUX_LIBS=


auto-all:
	case `uname -s` in \
	SunOS) echo Using SunOS configuration; \
	  make SYSCFLAGS="$(SOLARIS_CFLAGS)" SYSLIBS="$(SOLARIS_LIBS)" all;; \
	Linux) echo Using Linux configuration; \
	  make SYSCFLAGS="$(LINUX_CFLAGS)" SYSLIBS="$(LINUX_LIBS)" all;; \
	*) echo "unknown system"; exit 1;; \
	esac

all: test TAGS

# For adapting this Makefile to a different system, only the following
# definitions should need customizing:

OPENSSLDIR=../..
CC=gcc

SYSCFLAGS=whatever
SYSLIBS=whatever


#############################################################################
#
# SSLeay/OpenSSL imports
#
# OPENSSLDIR (set above) can be either the directory where OpenSSL is
# installed or the directory where it was compiled.

# We rely on having a new OpenSSL release where include files
# have names like <openssl/ssl.h> (not just <ssl.h>).
OPENSSLINCLUDES=-I$(OPENSSLDIR)/include

# libcrypto.a and libssl.a are directly in $(OPENSSLDIR) if this is
# the compile directory, or in $(OPENSSLDIR)/lib if we use an installed
# library.  With the following definition, we can handle either case.
OPENSSLLIBS=-L$(OPENSSLDIR) -L$(OPENSSLDIR)/lib -lssl -lcrypto


#############################################################################
#
# Stuff for handling the source files
#

SOURCES=easy-tls.c test.c
HEADERS=easy-tls.h test.h
DOCSandEXAMPLESetc=Makefile cert.pem cacerts.pem
EVERYTHING=$(SOURCES) $(HEADERS) $(DOCSandEXAMPLESetc)

ls: ls-l
ls-l:
	ls -l $(EVERYTHING)
# For RCS:
tag:
	-rcs -n_`date +%y%m%d`: $(EVERYTHING)
	rcs -nMYTAG $(EVERYTHING)
	rcs -nMYTAG: $(EVERYTHING)
diff:
	-rcsdiff -rMYTAG -u $(EVERYTHING)
today:
	-rcsdiff -r_`date +%y%m%d` -u $(EVERYTHING)
ident:
	for a in $(EVERYTHING); do ident $$a; done

# Distribution .tar:
easy-tls.tar.gz: $(EVERYTHING)
	tar cvf - $(EVERYTHING) | \
	gzip -9 > easy-tls.tar.gz

# Working .tar:
tls.tgz: $(EVERYTHING)
	tar cfv - `find . -type f -a ! -name '*.tgz' -a ! -name '*.tar.gz'` | \
	gzip -9 > tls.tgz

# For emacs:
etags: TAGS
TAGS: $(SOURCES) $(HEADERS)
	-etags $(SOURCES) $(HEADERS)


#############################################################################
#
# Compilation
#
# The following definitions are system dependent (and hence defined
# at the beginning of this Makefile, where they are more easily found):

### CC=gcc
### SYSCFLAGS=-Wall -pedantic -g -O2
### SYSLIBS=-lxnet

EXTRACFLAGS=-DTLS_APP=\"test.h\"
# EXTRACFLAGS=-DTLS_APP=\"test.h\" -DDEBUG_TLS

#
# The rest shouldn't need to be touched.
#
LDFLAGS=$(SYSLIBS) $(OPENSSLLIBS)
INCLUDES=$(OPENSSLINCLUDES)
CFLAGS=$(SYSCFLAGS) $(EXTRACFLAGS) $(INCLUDES)

OBJS=easy-tls.o test.o

clean:
	@rm -f test
	@rm -f TAGS
	@rm -f *.o
	@rm -f core

test: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o test

test.o: $(HEADERS)
easy-tls.o: $(HEADERS)
