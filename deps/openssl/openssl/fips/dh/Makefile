#
# OpenSSL/fips/dh/Makefile
#

DIR=	dh
TOP=	../..
CC=	cc
INCLUDES=
CFLAG=-g
INSTALL_PREFIX=
OPENSSLDIR=     /usr/local/ssl
INSTALLTOP=/usr/local/ssl
MAKEDEPPROG=	makedepend
MAKEDEPEND=	$(TOP)/util/domd $(TOP) -MD $(MAKEDEPPROG)
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=fips_dh_check.c fips_dh_gen.c fips_dh_key.c fips_dh_lib.c
LIBOBJ=fips_dh_check.o fips_dh_gen.o fips_dh_key.o fips_dh_lib.o

SRC= $(LIBSRC)

EXHEADER=
HEADER=	$(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd $(TOP); $(MAKE) DIRS=fips FDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	@echo $(LIBOBJ) > lib

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO

links:
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/include/openssl $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/test $(TEST)
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/apps $(APPS)

install:
	@headerlist="$(EXHEADER)"; for i in $$headerlist; \
	do  \
	  (cp $$i $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i; \
	  chmod 644 $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i ); \
	done

tags:
	ctags $(SRC)

tests:

fips_test:

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

depend:
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(SRC) $(TEST)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

fips_dh_check.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
fips_dh_check.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
fips_dh_check.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_dh_check.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
fips_dh_check.o: ../../include/openssl/opensslconf.h
fips_dh_check.o: ../../include/openssl/opensslv.h
fips_dh_check.o: ../../include/openssl/ossl_typ.h
fips_dh_check.o: ../../include/openssl/safestack.h
fips_dh_check.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
fips_dh_check.o: fips_dh_check.c
fips_dh_gen.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
fips_dh_gen.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
fips_dh_gen.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_dh_gen.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
fips_dh_gen.o: ../../include/openssl/opensslconf.h
fips_dh_gen.o: ../../include/openssl/opensslv.h
fips_dh_gen.o: ../../include/openssl/ossl_typ.h
fips_dh_gen.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
fips_dh_gen.o: ../../include/openssl/symhacks.h fips_dh_gen.c
fips_dh_key.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
fips_dh_key.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
fips_dh_key.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_dh_key.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
fips_dh_key.o: ../../include/openssl/opensslconf.h
fips_dh_key.o: ../../include/openssl/opensslv.h
fips_dh_key.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rand.h
fips_dh_key.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
fips_dh_key.o: ../../include/openssl/symhacks.h fips_dh_key.c
fips_dh_lib.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
fips_dh_lib.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
fips_dh_lib.o: ../../include/openssl/e_os2.h
fips_dh_lib.o: ../../include/openssl/opensslconf.h
fips_dh_lib.o: ../../include/openssl/opensslv.h
fips_dh_lib.o: ../../include/openssl/ossl_typ.h
fips_dh_lib.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
fips_dh_lib.o: ../../include/openssl/symhacks.h fips_dh_lib.c
