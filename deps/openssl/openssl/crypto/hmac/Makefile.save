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
LIBSRC=hmac.c hm_ameth.c hm_pmeth.c
LIBOBJ=hmac.o hm_ameth.o hm_pmeth.o

SRC= $(LIBSRC)

EXHEADER= hmac.h
HEADER=	$(EXHEADER)

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

hm_ameth.o: ../../e_os.h ../../include/openssl/asn1.h
hm_ameth.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
hm_ameth.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
hm_ameth.o: ../../include/openssl/err.h ../../include/openssl/evp.h
hm_ameth.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
hm_ameth.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
hm_ameth.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
hm_ameth.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
hm_ameth.o: ../../include/openssl/symhacks.h ../asn1/asn1_locl.h ../cryptlib.h
hm_ameth.o: hm_ameth.c
hm_pmeth.o: ../../e_os.h ../../include/openssl/asn1.h
hm_pmeth.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
hm_pmeth.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
hm_pmeth.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
hm_pmeth.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
hm_pmeth.o: ../../include/openssl/err.h ../../include/openssl/evp.h
hm_pmeth.o: ../../include/openssl/hmac.h ../../include/openssl/lhash.h
hm_pmeth.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
hm_pmeth.o: ../../include/openssl/opensslconf.h
hm_pmeth.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
hm_pmeth.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
hm_pmeth.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
hm_pmeth.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
hm_pmeth.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
hm_pmeth.o: ../cryptlib.h ../evp/evp_locl.h hm_pmeth.c
hmac.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
hmac.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
hmac.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
hmac.o: ../../include/openssl/evp.h ../../include/openssl/hmac.h
hmac.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
hmac.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
hmac.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
hmac.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
hmac.o: ../../include/openssl/symhacks.h ../cryptlib.h hmac.c
