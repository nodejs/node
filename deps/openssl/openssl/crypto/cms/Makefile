#
# OpenSSL/crypto/cms/Makefile
#

DIR=	cms
TOP=	../..
CC=	cc
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC= cms_lib.c cms_asn1.c cms_att.c cms_io.c cms_smime.c cms_err.c \
	cms_sd.c cms_dd.c cms_cd.c cms_env.c cms_enc.c cms_ess.c
LIBOBJ= cms_lib.o cms_asn1.o cms_att.o cms_io.o cms_smime.o cms_err.o \
	cms_sd.o cms_dd.o cms_cd.o cms_env.o cms_enc.o cms_ess.o

SRC= $(LIBSRC)

EXHEADER=  cms.h
HEADER=	cms_lcl.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

test:

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

cms_asn1.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
cms_asn1.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
cms_asn1.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
cms_asn1.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
cms_asn1.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
cms_asn1.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
cms_asn1.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
cms_asn1.o: ../../include/openssl/opensslconf.h
cms_asn1.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cms_asn1.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
cms_asn1.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
cms_asn1.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
cms_asn1.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
cms_asn1.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
cms_asn1.o: cms.h cms_asn1.c cms_lcl.h
cms_att.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
cms_att.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
cms_att.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
cms_att.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
cms_att.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
cms_att.o: ../../include/openssl/err.h ../../include/openssl/evp.h
cms_att.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
cms_att.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
cms_att.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cms_att.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
cms_att.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
cms_att.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
cms_att.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
cms_att.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
cms_att.o: cms.h cms_att.c cms_lcl.h
cms_cd.o: ../../e_os.h ../../include/openssl/asn1.h
cms_cd.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
cms_cd.o: ../../include/openssl/buffer.h ../../include/openssl/cms.h
cms_cd.o: ../../include/openssl/comp.h ../../include/openssl/conf.h
cms_cd.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
cms_cd.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
cms_cd.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
cms_cd.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
cms_cd.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
cms_cd.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
cms_cd.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pem.h
cms_cd.o: ../../include/openssl/pem2.h ../../include/openssl/pkcs7.h
cms_cd.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
cms_cd.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
cms_cd.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
cms_cd.o: ../../include/openssl/x509v3.h ../cryptlib.h cms_cd.c cms_lcl.h
cms_dd.o: ../../e_os.h ../../include/openssl/asn1.h
cms_dd.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
cms_dd.o: ../../include/openssl/buffer.h ../../include/openssl/cms.h
cms_dd.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
cms_dd.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
cms_dd.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
cms_dd.o: ../../include/openssl/err.h ../../include/openssl/evp.h
cms_dd.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
cms_dd.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
cms_dd.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cms_dd.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
cms_dd.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
cms_dd.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
cms_dd.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
cms_dd.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
cms_dd.o: ../cryptlib.h cms_dd.c cms_lcl.h
cms_enc.o: ../../e_os.h ../../include/openssl/asn1.h
cms_enc.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
cms_enc.o: ../../include/openssl/buffer.h ../../include/openssl/cms.h
cms_enc.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
cms_enc.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
cms_enc.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
cms_enc.o: ../../include/openssl/err.h ../../include/openssl/evp.h
cms_enc.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
cms_enc.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
cms_enc.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cms_enc.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
cms_enc.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
cms_enc.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
cms_enc.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
cms_enc.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
cms_enc.o: ../../include/openssl/x509v3.h ../cryptlib.h cms_enc.c cms_lcl.h
cms_env.o: ../../e_os.h ../../include/openssl/aes.h
cms_env.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
cms_env.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
cms_env.o: ../../include/openssl/cms.h ../../include/openssl/conf.h
cms_env.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
cms_env.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
cms_env.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
cms_env.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
cms_env.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
cms_env.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
cms_env.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pem.h
cms_env.o: ../../include/openssl/pem2.h ../../include/openssl/pkcs7.h
cms_env.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
cms_env.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
cms_env.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
cms_env.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
cms_env.o: ../asn1/asn1_locl.h ../cryptlib.h cms_env.c cms_lcl.h
cms_err.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
cms_err.o: ../../include/openssl/buffer.h ../../include/openssl/cms.h
cms_err.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
cms_err.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
cms_err.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
cms_err.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
cms_err.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
cms_err.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
cms_err.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
cms_err.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
cms_err.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
cms_err.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
cms_err.o: cms_err.c
cms_ess.o: ../../e_os.h ../../include/openssl/asn1.h
cms_ess.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
cms_ess.o: ../../include/openssl/buffer.h ../../include/openssl/cms.h
cms_ess.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
cms_ess.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
cms_ess.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
cms_ess.o: ../../include/openssl/err.h ../../include/openssl/evp.h
cms_ess.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
cms_ess.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
cms_ess.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cms_ess.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
cms_ess.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
cms_ess.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
cms_ess.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
cms_ess.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
cms_ess.o: ../../include/openssl/x509v3.h ../cryptlib.h cms_ess.c cms_lcl.h
cms_io.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
cms_io.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
cms_io.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
cms_io.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
cms_io.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
cms_io.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
cms_io.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
cms_io.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
cms_io.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pem.h
cms_io.o: ../../include/openssl/pem2.h ../../include/openssl/pkcs7.h
cms_io.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
cms_io.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
cms_io.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h cms.h
cms_io.o: cms_io.c cms_lcl.h
cms_lib.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
cms_lib.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
cms_lib.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
cms_lib.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
cms_lib.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
cms_lib.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
cms_lib.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
cms_lib.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
cms_lib.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pem.h
cms_lib.o: ../../include/openssl/pem2.h ../../include/openssl/pkcs7.h
cms_lib.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
cms_lib.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
cms_lib.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h cms.h
cms_lib.o: cms_lcl.h cms_lib.c
cms_sd.o: ../../e_os.h ../../include/openssl/asn1.h
cms_sd.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
cms_sd.o: ../../include/openssl/buffer.h ../../include/openssl/cms.h
cms_sd.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
cms_sd.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
cms_sd.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
cms_sd.o: ../../include/openssl/err.h ../../include/openssl/evp.h
cms_sd.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
cms_sd.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
cms_sd.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cms_sd.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
cms_sd.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
cms_sd.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
cms_sd.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
cms_sd.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
cms_sd.o: ../asn1/asn1_locl.h ../cryptlib.h cms_lcl.h cms_sd.c
cms_smime.o: ../../e_os.h ../../include/openssl/asn1.h
cms_smime.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
cms_smime.o: ../../include/openssl/buffer.h ../../include/openssl/cms.h
cms_smime.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
cms_smime.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
cms_smime.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
cms_smime.o: ../../include/openssl/err.h ../../include/openssl/evp.h
cms_smime.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
cms_smime.o: ../../include/openssl/objects.h
cms_smime.o: ../../include/openssl/opensslconf.h
cms_smime.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cms_smime.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
cms_smime.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
cms_smime.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
cms_smime.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
cms_smime.o: ../cryptlib.h cms_lcl.h cms_smime.c
