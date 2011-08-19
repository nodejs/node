#
# OpenSSL/crypto/evp/Makefile
#

DIR=	evp
TOP=	../..
CC=	cc
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=evp_test.c
TESTDATA=evptests.txt
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC= encode.c digest.c dig_eng.c evp_enc.c evp_key.c evp_acnf.c evp_cnf.c \
	e_des.c e_bf.c e_idea.c e_des3.c e_camellia.c\
	e_rc4.c e_aes.c names.c e_seed.c \
	e_xcbc_d.c e_rc2.c e_cast.c e_rc5.c enc_min.c \
	m_null.c m_md2.c m_md4.c m_md5.c m_sha.c m_sha1.c \
	m_dss.c m_dss1.c m_mdc2.c m_ripemd.c m_ecdsa.c\
	p_open.c p_seal.c p_sign.c p_verify.c p_lib.c p_enc.c p_dec.c \
	bio_md.c bio_b64.c bio_enc.c evp_err.c e_null.c \
	c_all.c c_allc.c c_alld.c evp_lib.c bio_ok.c \
	evp_pkey.c evp_pbe.c p5_crpt.c p5_crpt2.c \
	e_old.c

LIBOBJ=	encode.o digest.o dig_eng.o evp_enc.o evp_key.o evp_acnf.o evp_cnf.o \
	e_des.o e_bf.o e_idea.o e_des3.o e_camellia.o\
	e_rc4.o e_aes.o names.o e_seed.o \
	e_xcbc_d.o e_rc2.o e_cast.o e_rc5.o enc_min.o \
	m_null.o m_md2.o m_md4.o m_md5.o m_sha.o m_sha1.o \
	m_dss.o m_dss1.o m_mdc2.o m_ripemd.o m_ecdsa.o\
	p_open.o p_seal.o p_sign.o p_verify.o p_lib.o p_enc.o p_dec.o \
	bio_md.o bio_b64.o bio_enc.o evp_err.o e_null.o \
	c_all.o c_allc.o c_alld.o evp_lib.o bio_ok.o \
	evp_pkey.o evp_pbe.o p5_crpt.o p5_crpt2.o \
	e_old.o

SRC= $(LIBSRC)

EXHEADER= evp.h
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
	cp $(TESTDATA) ../../test
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
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(LIBSRC)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

bio_b64.o: ../../e_os.h ../../include/openssl/asn1.h
bio_b64.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
bio_b64.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
bio_b64.o: ../../include/openssl/err.h ../../include/openssl/evp.h
bio_b64.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
bio_b64.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
bio_b64.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
bio_b64.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
bio_b64.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
bio_b64.o: ../cryptlib.h bio_b64.c
bio_enc.o: ../../e_os.h ../../include/openssl/asn1.h
bio_enc.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
bio_enc.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
bio_enc.o: ../../include/openssl/err.h ../../include/openssl/evp.h
bio_enc.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
bio_enc.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
bio_enc.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
bio_enc.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
bio_enc.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
bio_enc.o: ../cryptlib.h bio_enc.c
bio_md.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
bio_md.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bio_md.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bio_md.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
bio_md.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
bio_md.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
bio_md.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bio_md.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bio_md.o: ../../include/openssl/symhacks.h ../cryptlib.h bio_md.c
bio_ok.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
bio_ok.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bio_ok.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bio_ok.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
bio_ok.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
bio_ok.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
bio_ok.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bio_ok.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
bio_ok.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
bio_ok.o: ../cryptlib.h bio_ok.c
c_all.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
c_all.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
c_all.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
c_all.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
c_all.o: ../../include/openssl/engine.h ../../include/openssl/err.h
c_all.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
c_all.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
c_all.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
c_all.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
c_all.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
c_all.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
c_all.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
c_all.o: ../../include/openssl/x509_vfy.h ../cryptlib.h c_all.c
c_allc.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
c_allc.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
c_allc.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
c_allc.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
c_allc.o: ../../include/openssl/err.h ../../include/openssl/evp.h
c_allc.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
c_allc.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
c_allc.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
c_allc.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs12.h
c_allc.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
c_allc.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
c_allc.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
c_allc.o: ../../include/openssl/x509_vfy.h ../cryptlib.h c_allc.c
c_alld.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
c_alld.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
c_alld.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
c_alld.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
c_alld.o: ../../include/openssl/err.h ../../include/openssl/evp.h
c_alld.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
c_alld.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
c_alld.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
c_alld.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs12.h
c_alld.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
c_alld.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
c_alld.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
c_alld.o: ../../include/openssl/x509_vfy.h ../cryptlib.h c_alld.c
dig_eng.o: ../../e_os.h ../../include/openssl/asn1.h
dig_eng.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
dig_eng.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
dig_eng.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
dig_eng.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
dig_eng.o: ../../include/openssl/err.h ../../include/openssl/evp.h
dig_eng.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
dig_eng.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
dig_eng.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
dig_eng.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
dig_eng.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
dig_eng.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
dig_eng.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
dig_eng.o: ../cryptlib.h dig_eng.c evp_locl.h
digest.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
digest.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
digest.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
digest.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
digest.o: ../../include/openssl/engine.h ../../include/openssl/err.h
digest.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
digest.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
digest.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
digest.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
digest.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
digest.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
digest.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
digest.o: ../../include/openssl/x509_vfy.h ../cryptlib.h digest.c evp_locl.h
e_aes.o: ../../include/openssl/aes.h ../../include/openssl/asn1.h
e_aes.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
e_aes.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_aes.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
e_aes.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
e_aes.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
e_aes.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_aes.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_aes.o: ../../include/openssl/symhacks.h e_aes.c evp_locl.h
e_bf.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_bf.o: ../../include/openssl/blowfish.h ../../include/openssl/buffer.h
e_bf.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
e_bf.o: ../../include/openssl/err.h ../../include/openssl/evp.h
e_bf.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
e_bf.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_bf.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
e_bf.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
e_bf.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
e_bf.o: ../cryptlib.h e_bf.c evp_locl.h
e_camellia.o: ../../include/openssl/opensslconf.h e_camellia.c
e_cast.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_cast.o: ../../include/openssl/buffer.h ../../include/openssl/cast.h
e_cast.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
e_cast.o: ../../include/openssl/err.h ../../include/openssl/evp.h
e_cast.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
e_cast.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_cast.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
e_cast.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
e_cast.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
e_cast.o: ../cryptlib.h e_cast.c evp_locl.h
e_des.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_des.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_des.o: ../../include/openssl/des.h ../../include/openssl/des_old.h
e_des.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_des.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
e_des.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
e_des.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
e_des.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_des.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
e_des.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
e_des.o: ../../include/openssl/ui.h ../../include/openssl/ui_compat.h
e_des.o: ../cryptlib.h e_des.c evp_locl.h
e_des3.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_des3.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_des3.o: ../../include/openssl/des.h ../../include/openssl/des_old.h
e_des3.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_des3.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
e_des3.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
e_des3.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
e_des3.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_des3.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
e_des3.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
e_des3.o: ../../include/openssl/ui.h ../../include/openssl/ui_compat.h
e_des3.o: ../cryptlib.h e_des3.c evp_locl.h
e_idea.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_idea.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_idea.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_idea.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
e_idea.o: ../../include/openssl/idea.h ../../include/openssl/lhash.h
e_idea.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_idea.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
e_idea.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
e_idea.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
e_idea.o: ../cryptlib.h e_idea.c evp_locl.h
e_null.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_null.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_null.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_null.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
e_null.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
e_null.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
e_null.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_null.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_null.o: ../../include/openssl/symhacks.h ../cryptlib.h e_null.c
e_old.o: e_old.c
e_rc2.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_rc2.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_rc2.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_rc2.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
e_rc2.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
e_rc2.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
e_rc2.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_rc2.o: ../../include/openssl/rc2.h ../../include/openssl/safestack.h
e_rc2.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
e_rc2.o: ../cryptlib.h e_rc2.c evp_locl.h
e_rc4.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_rc4.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_rc4.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_rc4.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
e_rc4.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
e_rc4.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
e_rc4.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_rc4.o: ../../include/openssl/rc4.h ../../include/openssl/safestack.h
e_rc4.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
e_rc4.o: ../cryptlib.h e_rc4.c evp_locl.h
e_rc5.o: ../../e_os.h ../../include/openssl/bio.h
e_rc5.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_rc5.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_rc5.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
e_rc5.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_rc5.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_rc5.o: ../../include/openssl/symhacks.h ../cryptlib.h e_rc5.c
e_seed.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_seed.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
e_seed.o: ../../include/openssl/err.h ../../include/openssl/evp.h
e_seed.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
e_seed.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_seed.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
e_seed.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
e_seed.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
e_seed.o: e_seed.c
e_xcbc_d.o: ../../e_os.h ../../include/openssl/asn1.h
e_xcbc_d.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
e_xcbc_d.o: ../../include/openssl/crypto.h ../../include/openssl/des.h
e_xcbc_d.o: ../../include/openssl/des_old.h ../../include/openssl/e_os2.h
e_xcbc_d.o: ../../include/openssl/err.h ../../include/openssl/evp.h
e_xcbc_d.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
e_xcbc_d.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_xcbc_d.o: ../../include/openssl/opensslconf.h
e_xcbc_d.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_xcbc_d.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_xcbc_d.o: ../../include/openssl/symhacks.h ../../include/openssl/ui.h
e_xcbc_d.o: ../../include/openssl/ui_compat.h ../cryptlib.h e_xcbc_d.c
enc_min.o: ../../e_os.h ../../include/openssl/asn1.h
enc_min.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
enc_min.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
enc_min.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
enc_min.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
enc_min.o: ../../include/openssl/err.h ../../include/openssl/evp.h
enc_min.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
enc_min.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
enc_min.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
enc_min.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
enc_min.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
enc_min.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
enc_min.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
enc_min.o: ../../include/openssl/x509_vfy.h ../cryptlib.h enc_min.c evp_locl.h
encode.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
encode.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
encode.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
encode.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
encode.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
encode.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
encode.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
encode.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
encode.o: ../../include/openssl/symhacks.h ../cryptlib.h encode.c
evp_acnf.o: ../../e_os.h ../../include/openssl/asn1.h
evp_acnf.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
evp_acnf.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
evp_acnf.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
evp_acnf.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
evp_acnf.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
evp_acnf.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
evp_acnf.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
evp_acnf.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
evp_acnf.o: ../../include/openssl/symhacks.h ../cryptlib.h evp_acnf.c
evp_cnf.o: ../../e_os.h ../../include/openssl/asn1.h
evp_cnf.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
evp_cnf.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
evp_cnf.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
evp_cnf.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
evp_cnf.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
evp_cnf.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
evp_cnf.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
evp_cnf.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
evp_cnf.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
evp_cnf.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
evp_cnf.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
evp_cnf.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
evp_cnf.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
evp_cnf.o: ../cryptlib.h evp_cnf.c
evp_enc.o: ../../e_os.h ../../include/openssl/asn1.h
evp_enc.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
evp_enc.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
evp_enc.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
evp_enc.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
evp_enc.o: ../../include/openssl/err.h ../../include/openssl/evp.h
evp_enc.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
evp_enc.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
evp_enc.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
evp_enc.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
evp_enc.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
evp_enc.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
evp_enc.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
evp_enc.o: ../../include/openssl/x509_vfy.h ../cryptlib.h evp_enc.c evp_locl.h
evp_err.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
evp_err.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
evp_err.o: ../../include/openssl/err.h ../../include/openssl/evp.h
evp_err.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
evp_err.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
evp_err.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
evp_err.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
evp_err.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
evp_err.o: evp_err.c
evp_key.o: ../../e_os.h ../../include/openssl/asn1.h
evp_key.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
evp_key.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
evp_key.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
evp_key.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
evp_key.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
evp_key.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
evp_key.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
evp_key.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
evp_key.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
evp_key.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
evp_key.o: ../../include/openssl/symhacks.h ../../include/openssl/ui.h
evp_key.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
evp_key.o: ../cryptlib.h evp_key.c
evp_lib.o: ../../e_os.h ../../include/openssl/asn1.h
evp_lib.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
evp_lib.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
evp_lib.o: ../../include/openssl/err.h ../../include/openssl/evp.h
evp_lib.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
evp_lib.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
evp_lib.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
evp_lib.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
evp_lib.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
evp_lib.o: ../cryptlib.h evp_lib.c
evp_pbe.o: ../../e_os.h ../../include/openssl/asn1.h
evp_pbe.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
evp_pbe.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
evp_pbe.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
evp_pbe.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
evp_pbe.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
evp_pbe.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
evp_pbe.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
evp_pbe.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
evp_pbe.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
evp_pbe.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
evp_pbe.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
evp_pbe.o: ../../include/openssl/x509_vfy.h ../cryptlib.h evp_pbe.c
evp_pkey.o: ../../e_os.h ../../include/openssl/asn1.h
evp_pkey.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
evp_pkey.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
evp_pkey.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
evp_pkey.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
evp_pkey.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
evp_pkey.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
evp_pkey.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
evp_pkey.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
evp_pkey.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
evp_pkey.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
evp_pkey.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
evp_pkey.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
evp_pkey.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
evp_pkey.o: ../../include/openssl/x509_vfy.h ../cryptlib.h evp_pkey.c
m_dss.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_dss.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_dss.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
m_dss.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
m_dss.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
m_dss.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
m_dss.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
m_dss.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_dss.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_dss.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
m_dss.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_dss.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
m_dss.o: ../../include/openssl/x509_vfy.h ../cryptlib.h m_dss.c
m_dss1.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_dss1.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_dss1.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
m_dss1.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
m_dss1.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
m_dss1.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
m_dss1.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
m_dss1.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_dss1.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_dss1.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
m_dss1.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_dss1.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
m_dss1.o: ../../include/openssl/x509_vfy.h ../cryptlib.h m_dss1.c
m_ecdsa.o: ../../e_os.h ../../include/openssl/asn1.h
m_ecdsa.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
m_ecdsa.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
m_ecdsa.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
m_ecdsa.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
m_ecdsa.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
m_ecdsa.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
m_ecdsa.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_ecdsa.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_ecdsa.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
m_ecdsa.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_ecdsa.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
m_ecdsa.o: ../../include/openssl/x509_vfy.h ../cryptlib.h m_ecdsa.c
m_md2.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_md2.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_md2.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_md2.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_md2.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_md2.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
m_md2.o: ../../include/openssl/md2.h ../../include/openssl/obj_mac.h
m_md2.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_md2.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_md2.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
m_md2.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
m_md2.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
m_md2.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
m_md2.o: ../cryptlib.h evp_locl.h m_md2.c
m_md4.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_md4.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_md4.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_md4.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_md4.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_md4.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
m_md4.o: ../../include/openssl/md4.h ../../include/openssl/obj_mac.h
m_md4.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_md4.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_md4.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
m_md4.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
m_md4.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
m_md4.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
m_md4.o: ../cryptlib.h evp_locl.h m_md4.c
m_md5.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_md5.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_md5.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_md5.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_md5.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_md5.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
m_md5.o: ../../include/openssl/md5.h ../../include/openssl/obj_mac.h
m_md5.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_md5.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_md5.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
m_md5.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
m_md5.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
m_md5.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
m_md5.o: ../cryptlib.h evp_locl.h m_md5.c
m_mdc2.o: ../../e_os.h ../../include/openssl/bio.h
m_mdc2.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_mdc2.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
m_mdc2.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
m_mdc2.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_mdc2.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
m_mdc2.o: ../../include/openssl/symhacks.h ../cryptlib.h evp_locl.h m_mdc2.c
m_null.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_null.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_null.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_null.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_null.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_null.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
m_null.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
m_null.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
m_null.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
m_null.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
m_null.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
m_null.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
m_null.o: ../cryptlib.h m_null.c
m_ripemd.o: ../../e_os.h ../../include/openssl/asn1.h
m_ripemd.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
m_ripemd.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
m_ripemd.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
m_ripemd.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
m_ripemd.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
m_ripemd.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
m_ripemd.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_ripemd.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_ripemd.o: ../../include/openssl/pkcs7.h ../../include/openssl/ripemd.h
m_ripemd.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
m_ripemd.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_ripemd.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
m_ripemd.o: ../../include/openssl/x509_vfy.h ../cryptlib.h m_ripemd.c
m_sha.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_sha.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_sha.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_sha.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_sha.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_sha.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
m_sha.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
m_sha.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
m_sha.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
m_sha.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
m_sha.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_sha.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
m_sha.o: ../../include/openssl/x509_vfy.h ../cryptlib.h evp_locl.h m_sha.c
m_sha1.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_sha1.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_sha1.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_sha1.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_sha1.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_sha1.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
m_sha1.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
m_sha1.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
m_sha1.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
m_sha1.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
m_sha1.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_sha1.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
m_sha1.o: ../../include/openssl/x509_vfy.h ../cryptlib.h m_sha1.c
names.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
names.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
names.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
names.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
names.o: ../../include/openssl/err.h ../../include/openssl/evp.h
names.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
names.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
names.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
names.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
names.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
names.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
names.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
names.o: ../cryptlib.h names.c
p5_crpt.o: ../../e_os.h ../../include/openssl/asn1.h
p5_crpt.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
p5_crpt.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
p5_crpt.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
p5_crpt.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
p5_crpt.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
p5_crpt.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p5_crpt.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p5_crpt.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p5_crpt.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
p5_crpt.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p5_crpt.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p5_crpt.o: ../../include/openssl/x509_vfy.h ../cryptlib.h p5_crpt.c
p5_crpt2.o: ../../e_os.h ../../include/openssl/asn1.h
p5_crpt2.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
p5_crpt2.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
p5_crpt2.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
p5_crpt2.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
p5_crpt2.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
p5_crpt2.o: ../../include/openssl/hmac.h ../../include/openssl/lhash.h
p5_crpt2.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
p5_crpt2.o: ../../include/openssl/opensslconf.h
p5_crpt2.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p5_crpt2.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
p5_crpt2.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p5_crpt2.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p5_crpt2.o: ../../include/openssl/x509_vfy.h ../cryptlib.h p5_crpt2.c
p_dec.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
p_dec.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p_dec.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p_dec.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p_dec.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p_dec.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
p_dec.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
p_dec.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
p_dec.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
p_dec.o: ../../include/openssl/rand.h ../../include/openssl/rsa.h
p_dec.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
p_dec.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
p_dec.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
p_dec.o: ../cryptlib.h p_dec.c
p_enc.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
p_enc.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p_enc.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p_enc.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p_enc.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p_enc.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
p_enc.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
p_enc.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
p_enc.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
p_enc.o: ../../include/openssl/rand.h ../../include/openssl/rsa.h
p_enc.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
p_enc.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
p_enc.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
p_enc.o: ../cryptlib.h p_enc.c
p_lib.o: ../../e_os.h ../../include/openssl/asn1.h
p_lib.o: ../../include/openssl/asn1_mac.h ../../include/openssl/bio.h
p_lib.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
p_lib.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
p_lib.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
p_lib.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
p_lib.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
p_lib.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
p_lib.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p_lib.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p_lib.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
p_lib.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
p_lib.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
p_lib.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
p_lib.o: ../cryptlib.h p_lib.c
p_open.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
p_open.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p_open.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p_open.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p_open.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p_open.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
p_open.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
p_open.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
p_open.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
p_open.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
p_open.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p_open.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p_open.o: ../../include/openssl/x509_vfy.h ../cryptlib.h p_open.c
p_seal.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
p_seal.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p_seal.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p_seal.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p_seal.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p_seal.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
p_seal.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
p_seal.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
p_seal.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
p_seal.o: ../../include/openssl/rand.h ../../include/openssl/rsa.h
p_seal.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
p_seal.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
p_seal.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
p_seal.o: ../cryptlib.h p_seal.c
p_sign.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
p_sign.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p_sign.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p_sign.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p_sign.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p_sign.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
p_sign.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
p_sign.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
p_sign.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
p_sign.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
p_sign.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
p_sign.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
p_sign.o: ../cryptlib.h p_sign.c
p_verify.o: ../../e_os.h ../../include/openssl/asn1.h
p_verify.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
p_verify.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
p_verify.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
p_verify.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
p_verify.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
p_verify.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p_verify.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p_verify.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p_verify.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
p_verify.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p_verify.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p_verify.o: ../../include/openssl/x509_vfy.h ../cryptlib.h p_verify.c
