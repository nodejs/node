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
cms_sd.o: ../cryptlib.h cms_lcl.h cms_sd.c
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
