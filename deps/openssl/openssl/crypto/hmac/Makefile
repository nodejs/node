#
# OpenSSL/crypto/md/Makefile
#

DIR=	hmac
TOP=	../..
CC=	cc
INCLUDES=
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=hmactest.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=hmac.c
LIBOBJ=hmac.o

SRC= $(LIBSRC)

EXHEADER= hmac.h
HEADER=	$(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(ARX) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO

links:
	@$(PERL) $(TOP)/util/mklink.pl ../../include/openssl $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl ../../test $(TEST)
	@$(PERL) $(TOP)/util/mklink.pl ../../apps $(APPS)

install:
	@[ -n "$(INSTALLTOP)" ] # should be set by top Makefile...
	@headerlist="$(EXHEADER)"; for i in $$headerlist ; \
	do  \
	(cp $$i $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i; \
	chmod 644 $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i ); \
	done;

tags:
	ctags $(SRC)

tests:

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

depend:
	@[ -n "$(MAKEDEPEND)" ] # should be set by upper Makefile...
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(PROGS) $(LIBSRC)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

hmac.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
hmac.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
hmac.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
hmac.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
hmac.o: ../../include/openssl/hmac.h ../../include/openssl/lhash.h
hmac.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
hmac.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
hmac.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
hmac.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
hmac.o: ../cryptlib.h hmac.c
