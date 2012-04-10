#
# OpenSSL/crypto/rc2/Makefile
#

DIR=	rc2
TOP=	../..
CC=	cc
INCLUDES=
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=rc2test.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=rc2_ecb.c rc2_skey.c rc2_cbc.c rc2cfb64.c rc2ofb64.c
LIBOBJ=rc2_ecb.o rc2_skey.o rc2_cbc.o rc2cfb64.o rc2ofb64.o

SRC= $(LIBSRC)

EXHEADER= rc2.h
HEADER=	rc2_locl.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
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

rc2_cbc.o: ../../include/openssl/opensslconf.h ../../include/openssl/rc2.h
rc2_cbc.o: rc2_cbc.c rc2_locl.h
rc2_ecb.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
rc2_ecb.o: ../../include/openssl/rc2.h rc2_ecb.c rc2_locl.h
rc2_skey.o: ../../include/openssl/opensslconf.h ../../include/openssl/rc2.h
rc2_skey.o: rc2_locl.h rc2_skey.c
rc2cfb64.o: ../../include/openssl/opensslconf.h ../../include/openssl/rc2.h
rc2cfb64.o: rc2_locl.h rc2cfb64.c
rc2ofb64.o: ../../include/openssl/opensslconf.h ../../include/openssl/rc2.h
rc2ofb64.o: rc2_locl.h rc2ofb64.c
