#
# OpenSSL/crypto/rsa/Makefile
#

DIR=	rsa
TOP=	../..
CC=	cc
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=rsa_test.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC= rsa_eay.c rsa_gen.c rsa_lib.c rsa_sign.c rsa_saos.c rsa_err.c \
	rsa_pk1.c rsa_ssl.c rsa_none.c rsa_oaep.c rsa_chk.c rsa_null.c \
	rsa_pss.c rsa_x931.c rsa_x931g.c rsa_asn1.c rsa_depr.c rsa_eng.c
LIBOBJ= rsa_eay.o rsa_gen.o rsa_lib.o rsa_sign.o rsa_saos.o rsa_err.o \
	rsa_pk1.o rsa_ssl.o rsa_none.o rsa_oaep.o rsa_chk.o rsa_null.o \
	rsa_pss.o rsa_x931.o rsa_x931g.o rsa_asn1.o rsa_depr.o rsa_eng.o

SRC= $(LIBSRC)

EXHEADER= rsa.h
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
	rm -f *.o */*.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

rsa_asn1.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_asn1.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
rsa_asn1.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
rsa_asn1.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
rsa_asn1.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
rsa_asn1.o: ../../include/openssl/opensslconf.h
rsa_asn1.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_asn1.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
rsa_asn1.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rsa_asn1.o: ../cryptlib.h rsa_asn1.c
rsa_chk.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
rsa_chk.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
rsa_chk.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rsa_chk.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rsa_chk.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_chk.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
rsa_chk.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rsa_chk.o: rsa_chk.c
rsa_depr.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_depr.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_depr.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_depr.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rsa_depr.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rsa_depr.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_depr.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
rsa_depr.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rsa_depr.o: ../cryptlib.h rsa_depr.c
rsa_eay.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_eay.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_eay.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_eay.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rsa_eay.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rsa_eay.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_eay.o: ../../include/openssl/rand.h ../../include/openssl/rsa.h
rsa_eay.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
rsa_eay.o: ../../include/openssl/symhacks.h ../cryptlib.h rsa_eay.c
rsa_eng.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_eng.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_eng.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_eng.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
rsa_eng.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
rsa_eng.o: ../../include/openssl/engine.h ../../include/openssl/err.h
rsa_eng.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
rsa_eng.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
rsa_eng.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
rsa_eng.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_eng.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
rsa_eng.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
rsa_eng.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
rsa_eng.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
rsa_eng.o: ../../include/openssl/x509_vfy.h ../cryptlib.h rsa_eng.c
rsa_err.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
rsa_err.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
rsa_err.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
rsa_err.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
rsa_err.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rsa.h
rsa_err.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
rsa_err.o: ../../include/openssl/symhacks.h rsa_err.c
rsa_gen.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_gen.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_gen.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_gen.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rsa_gen.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rsa_gen.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_gen.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
rsa_gen.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rsa_gen.o: ../cryptlib.h rsa_gen.c
rsa_lib.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_lib.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_lib.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
rsa_lib.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
rsa_lib.o: ../../include/openssl/engine.h ../../include/openssl/err.h
rsa_lib.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
rsa_lib.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
rsa_lib.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
rsa_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_lib.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
rsa_lib.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
rsa_lib.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
rsa_lib.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
rsa_lib.o: ../../include/openssl/x509_vfy.h ../cryptlib.h rsa_lib.c
rsa_none.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_none.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_none.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_none.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rsa_none.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rsa_none.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_none.o: ../../include/openssl/rand.h ../../include/openssl/rsa.h
rsa_none.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
rsa_none.o: ../../include/openssl/symhacks.h ../cryptlib.h rsa_none.c
rsa_null.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_null.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_null.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_null.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rsa_null.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rsa_null.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_null.o: ../../include/openssl/rand.h ../../include/openssl/rsa.h
rsa_null.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
rsa_null.o: ../../include/openssl/symhacks.h ../cryptlib.h rsa_null.c
rsa_oaep.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_oaep.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_oaep.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_oaep.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rsa_oaep.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
rsa_oaep.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
rsa_oaep.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
rsa_oaep.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_oaep.o: ../../include/openssl/rand.h ../../include/openssl/rsa.h
rsa_oaep.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
rsa_oaep.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rsa_oaep.o: ../cryptlib.h rsa_oaep.c
rsa_pk1.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_pk1.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_pk1.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_pk1.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rsa_pk1.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rsa_pk1.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_pk1.o: ../../include/openssl/rand.h ../../include/openssl/rsa.h
rsa_pk1.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
rsa_pk1.o: ../../include/openssl/symhacks.h ../cryptlib.h rsa_pk1.c
rsa_pss.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_pss.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_pss.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_pss.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rsa_pss.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
rsa_pss.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
rsa_pss.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
rsa_pss.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_pss.o: ../../include/openssl/rand.h ../../include/openssl/rsa.h
rsa_pss.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
rsa_pss.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rsa_pss.o: ../cryptlib.h rsa_pss.c
rsa_saos.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_saos.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_saos.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_saos.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
rsa_saos.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
rsa_saos.o: ../../include/openssl/err.h ../../include/openssl/evp.h
rsa_saos.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
rsa_saos.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
rsa_saos.o: ../../include/openssl/opensslconf.h
rsa_saos.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_saos.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
rsa_saos.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
rsa_saos.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rsa_saos.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
rsa_saos.o: ../cryptlib.h rsa_saos.c
rsa_sign.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_sign.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_sign.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_sign.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
rsa_sign.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
rsa_sign.o: ../../include/openssl/err.h ../../include/openssl/evp.h
rsa_sign.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
rsa_sign.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
rsa_sign.o: ../../include/openssl/opensslconf.h
rsa_sign.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_sign.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
rsa_sign.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
rsa_sign.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rsa_sign.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
rsa_sign.o: ../cryptlib.h rsa_sign.c
rsa_ssl.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_ssl.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_ssl.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_ssl.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rsa_ssl.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rsa_ssl.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_ssl.o: ../../include/openssl/rand.h ../../include/openssl/rsa.h
rsa_ssl.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
rsa_ssl.o: ../../include/openssl/symhacks.h ../cryptlib.h rsa_ssl.c
rsa_x931.o: ../../e_os.h ../../include/openssl/asn1.h
rsa_x931.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
rsa_x931.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rsa_x931.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rsa_x931.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
rsa_x931.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
rsa_x931.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_x931.o: ../../include/openssl/rand.h ../../include/openssl/rsa.h
rsa_x931.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
rsa_x931.o: ../../include/openssl/symhacks.h ../cryptlib.h rsa_x931.c
rsa_x931g.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
rsa_x931g.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
rsa_x931g.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rsa_x931g.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rsa_x931g.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rsa_x931g.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
rsa_x931g.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rsa_x931g.o: rsa_x931g.c
