#
# OpenSSL/crypto/conf/Makefile
#

DIR=	conf
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
LIBSRC= conf_err.c conf_lib.c conf_api.c conf_def.c conf_mod.c \
	 conf_mall.c conf_sap.c

LIBOBJ=	conf_err.o conf_lib.o conf_api.o conf_def.o conf_mod.o \
	conf_mall.o conf_sap.o

SRC= $(LIBSRC)

EXHEADER= conf.h conf_api.h
HEADER=	conf_def.h $(EXHEADER)

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
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(LIBSRC)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

conf_api.o: ../../e_os.h ../../include/openssl/bio.h
conf_api.o: ../../include/openssl/conf.h ../../include/openssl/conf_api.h
conf_api.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
conf_api.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
conf_api.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
conf_api.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
conf_api.o: ../../include/openssl/symhacks.h conf_api.c
conf_def.o: ../../e_os.h ../../include/openssl/bio.h
conf_def.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
conf_def.o: ../../include/openssl/conf_api.h ../../include/openssl/crypto.h
conf_def.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
conf_def.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
conf_def.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
conf_def.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
conf_def.o: ../../include/openssl/symhacks.h ../cryptlib.h conf_def.c
conf_def.o: conf_def.h
conf_err.o: ../../include/openssl/bio.h ../../include/openssl/conf.h
conf_err.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
conf_err.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
conf_err.o: ../../include/openssl/opensslconf.h
conf_err.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
conf_err.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
conf_err.o: ../../include/openssl/symhacks.h conf_err.c
conf_lib.o: ../../include/openssl/bio.h ../../include/openssl/conf.h
conf_lib.o: ../../include/openssl/conf_api.h ../../include/openssl/crypto.h
conf_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
conf_lib.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
conf_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
conf_lib.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
conf_lib.o: ../../include/openssl/symhacks.h conf_lib.c
conf_mall.o: ../../e_os.h ../../include/openssl/asn1.h
conf_mall.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
conf_mall.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
conf_mall.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
conf_mall.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
conf_mall.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
conf_mall.o: ../../include/openssl/err.h ../../include/openssl/evp.h
conf_mall.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
conf_mall.o: ../../include/openssl/objects.h
conf_mall.o: ../../include/openssl/opensslconf.h
conf_mall.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
conf_mall.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
conf_mall.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
conf_mall.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
conf_mall.o: ../../include/openssl/x509_vfy.h ../cryptlib.h conf_mall.c
conf_mod.o: ../../e_os.h ../../include/openssl/asn1.h
conf_mod.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
conf_mod.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
conf_mod.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
conf_mod.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
conf_mod.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
conf_mod.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
conf_mod.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
conf_mod.o: ../../include/openssl/opensslconf.h
conf_mod.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
conf_mod.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
conf_mod.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
conf_mod.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
conf_mod.o: ../../include/openssl/x509_vfy.h ../cryptlib.h conf_mod.c
conf_sap.o: ../../e_os.h ../../include/openssl/asn1.h
conf_sap.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
conf_sap.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
conf_sap.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
conf_sap.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
conf_sap.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
conf_sap.o: ../../include/openssl/err.h ../../include/openssl/evp.h
conf_sap.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
conf_sap.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
conf_sap.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
conf_sap.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
conf_sap.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
conf_sap.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
conf_sap.o: ../../include/openssl/x509_vfy.h ../cryptlib.h conf_sap.c
