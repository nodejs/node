#
# OpenSSL/crypto/dh/Makefile
#

DIR=	dh
TOP=	../..
CC=	cc
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST= dhtest.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC= dh_asn1.c dh_gen.c dh_key.c dh_lib.c dh_check.c dh_err.c dh_depr.c \
	dh_ameth.c dh_pmeth.c dh_prn.c dh_rfc5114.c dh_kdf.c
LIBOBJ= dh_asn1.o dh_gen.o dh_key.o dh_lib.o dh_check.o dh_err.o dh_depr.o \
	dh_ameth.o dh_pmeth.o dh_prn.o dh_rfc5114.o dh_kdf.o

SRC= $(LIBSRC)

EXHEADER= dh.h
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

update: depend

depend:
	@[ -n "$(MAKEDEPEND)" ] # should be set by upper Makefile...
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(PROGS) $(LIBSRC)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o */*.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

dh_ameth.o: ../../e_os.h ../../include/openssl/asn1.h
dh_ameth.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
dh_ameth.o: ../../include/openssl/buffer.h ../../include/openssl/cms.h
dh_ameth.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
dh_ameth.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
dh_ameth.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
dh_ameth.o: ../../include/openssl/err.h ../../include/openssl/evp.h
dh_ameth.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
dh_ameth.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
dh_ameth.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
dh_ameth.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
dh_ameth.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
dh_ameth.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
dh_ameth.o: ../../include/openssl/x509_vfy.h ../asn1/asn1_locl.h ../cryptlib.h
dh_ameth.o: dh_ameth.c
dh_asn1.o: ../../e_os.h ../../include/openssl/asn1.h
dh_asn1.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
dh_asn1.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
dh_asn1.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
dh_asn1.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
dh_asn1.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
dh_asn1.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
dh_asn1.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
dh_asn1.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
dh_asn1.o: ../../include/openssl/symhacks.h ../cryptlib.h dh_asn1.c
dh_check.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
dh_check.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
dh_check.o: ../../include/openssl/dh.h ../../include/openssl/e_os2.h
dh_check.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dh_check.o: ../../include/openssl/opensslconf.h
dh_check.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
dh_check.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
dh_check.o: ../../include/openssl/symhacks.h ../cryptlib.h dh_check.c
dh_depr.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
dh_depr.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
dh_depr.o: ../../include/openssl/dh.h ../../include/openssl/e_os2.h
dh_depr.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dh_depr.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
dh_depr.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
dh_depr.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
dh_depr.o: ../cryptlib.h dh_depr.c
dh_err.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
dh_err.o: ../../include/openssl/dh.h ../../include/openssl/e_os2.h
dh_err.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dh_err.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
dh_err.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
dh_err.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
dh_err.o: dh_err.c
dh_gen.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
dh_gen.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
dh_gen.o: ../../include/openssl/dh.h ../../include/openssl/e_os2.h
dh_gen.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dh_gen.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
dh_gen.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
dh_gen.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
dh_gen.o: ../cryptlib.h dh_gen.c
dh_kdf.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
dh_kdf.o: ../../include/openssl/buffer.h ../../include/openssl/cms.h
dh_kdf.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
dh_kdf.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
dh_kdf.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
dh_kdf.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
dh_kdf.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
dh_kdf.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
dh_kdf.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
dh_kdf.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
dh_kdf.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
dh_kdf.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
dh_kdf.o: dh_kdf.c
dh_key.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
dh_key.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
dh_key.o: ../../include/openssl/dh.h ../../include/openssl/e_os2.h
dh_key.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dh_key.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
dh_key.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rand.h
dh_key.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
dh_key.o: ../../include/openssl/symhacks.h ../cryptlib.h dh_key.c
dh_lib.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
dh_lib.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
dh_lib.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
dh_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
dh_lib.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
dh_lib.o: ../../include/openssl/engine.h ../../include/openssl/err.h
dh_lib.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
dh_lib.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
dh_lib.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
dh_lib.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
dh_lib.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
dh_lib.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
dh_lib.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
dh_lib.o: ../cryptlib.h dh_lib.c
dh_pmeth.o: ../../e_os.h ../../include/openssl/asn1.h
dh_pmeth.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
dh_pmeth.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
dh_pmeth.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
dh_pmeth.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
dh_pmeth.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
dh_pmeth.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
dh_pmeth.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
dh_pmeth.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
dh_pmeth.o: ../../include/openssl/opensslconf.h
dh_pmeth.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
dh_pmeth.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
dh_pmeth.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
dh_pmeth.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
dh_pmeth.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../evp/evp_locl.h
dh_pmeth.o: dh_pmeth.c
dh_prn.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
dh_prn.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
dh_prn.o: ../../include/openssl/dh.h ../../include/openssl/e_os2.h
dh_prn.o: ../../include/openssl/err.h ../../include/openssl/evp.h
dh_prn.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
dh_prn.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
dh_prn.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
dh_prn.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
dh_prn.o: ../../include/openssl/symhacks.h ../cryptlib.h dh_prn.c
dh_rfc5114.o: ../../e_os.h ../../include/openssl/bio.h
dh_rfc5114.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
dh_rfc5114.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
dh_rfc5114.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
dh_rfc5114.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
dh_rfc5114.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
dh_rfc5114.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
dh_rfc5114.o: ../../include/openssl/symhacks.h ../cryptlib.h dh_rfc5114.c
