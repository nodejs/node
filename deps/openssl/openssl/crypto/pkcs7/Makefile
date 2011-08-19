#
# OpenSSL/crypto/pkcs7/Makefile
#

DIR=	pkcs7
TOP=	../..
CC=	cc
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

PEX_LIBS=
EX_LIBS=
 
CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile README
TEST=
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=	pk7_asn1.c pk7_lib.c pkcs7err.c pk7_doit.c pk7_smime.c pk7_attr.c \
	pk7_mime.c
LIBOBJ= pk7_asn1.o pk7_lib.o pkcs7err.o pk7_doit.o pk7_smime.o pk7_attr.o \
	pk7_mime.o

SRC= $(LIBSRC)

EXHEADER=  pkcs7.h
HEADER=	$(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

test:

all:	lib

testapps: enc dec sign verify

enc: enc.o lib
	$(CC) $(CFLAGS) -o enc enc.o $(PEX_LIBS) $(LIB) $(EX_LIBS)

dec: dec.o lib
	$(CC) $(CFLAGS) -o dec dec.o $(PEX_LIBS) $(LIB) $(EX_LIBS)

sign: sign.o lib
	$(CC) $(CFLAGS) -o sign sign.o $(PEX_LIBS) $(LIB) $(EX_LIBS)

verify: verify.o example.o lib
	$(CC) $(CFLAGS) -o verify verify.o $(PEX_LIBS) example.o $(LIB) $(EX_LIBS)

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
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff enc dec sign verify

# DO NOT DELETE THIS LINE -- make depend depends on it.

pk7_asn1.o: ../../e_os.h ../../include/openssl/asn1.h
pk7_asn1.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
pk7_asn1.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pk7_asn1.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pk7_asn1.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pk7_asn1.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pk7_asn1.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
pk7_asn1.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pk7_asn1.o: ../../include/openssl/opensslconf.h
pk7_asn1.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pk7_asn1.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
pk7_asn1.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pk7_asn1.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pk7_asn1.o: ../../include/openssl/x509_vfy.h ../cryptlib.h pk7_asn1.c
pk7_attr.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pk7_attr.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pk7_attr.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pk7_attr.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pk7_attr.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pk7_attr.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
pk7_attr.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pk7_attr.o: ../../include/openssl/opensslconf.h
pk7_attr.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pk7_attr.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
pk7_attr.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
pk7_attr.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pk7_attr.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pk7_attr.o: ../../include/openssl/x509_vfy.h pk7_attr.c
pk7_doit.o: ../../e_os.h ../../include/openssl/asn1.h
pk7_doit.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
pk7_doit.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
pk7_doit.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pk7_doit.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pk7_doit.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pk7_doit.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
pk7_doit.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pk7_doit.o: ../../include/openssl/opensslconf.h
pk7_doit.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pk7_doit.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
pk7_doit.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
pk7_doit.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
pk7_doit.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
pk7_doit.o: ../../include/openssl/x509v3.h ../cryptlib.h pk7_doit.c
pk7_lib.o: ../../e_os.h ../../include/openssl/asn1.h
pk7_lib.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
pk7_lib.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
pk7_lib.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
pk7_lib.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
pk7_lib.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
pk7_lib.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
pk7_lib.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
pk7_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pk7_lib.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
pk7_lib.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pk7_lib.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pk7_lib.o: ../../include/openssl/x509_vfy.h ../cryptlib.h pk7_lib.c
pk7_mime.o: ../../e_os.h ../../include/openssl/asn1.h
pk7_mime.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
pk7_mime.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
pk7_mime.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
pk7_mime.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
pk7_mime.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
pk7_mime.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
pk7_mime.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
pk7_mime.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pk7_mime.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
pk7_mime.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
pk7_mime.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
pk7_mime.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
pk7_mime.o: ../cryptlib.h pk7_mime.c
pk7_smime.o: ../../e_os.h ../../include/openssl/asn1.h
pk7_smime.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
pk7_smime.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
pk7_smime.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pk7_smime.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pk7_smime.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pk7_smime.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
pk7_smime.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pk7_smime.o: ../../include/openssl/opensslconf.h
pk7_smime.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pk7_smime.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
pk7_smime.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pk7_smime.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pk7_smime.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
pk7_smime.o: ../cryptlib.h pk7_smime.c
pkcs7err.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pkcs7err.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
pkcs7err.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
pkcs7err.o: ../../include/openssl/opensslconf.h
pkcs7err.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pkcs7err.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
pkcs7err.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
pkcs7err.o: pkcs7err.c
