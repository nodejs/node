#
# OpenSSL/crypto/cmac/Makefile
#

DIR=	cmac
TOP=	../..
CC=	cc
INCLUDES=
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=cmac.c cm_ameth.c cm_pmeth.c
LIBOBJ=cmac.o cm_ameth.o cm_pmeth.o

SRC= $(LIBSRC)

EXHEADER= cmac.h
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
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

cm_ameth.o: ../../e_os.h ../../include/openssl/asn1.h
cm_ameth.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
cm_ameth.o: ../../include/openssl/cmac.h ../../include/openssl/crypto.h
cm_ameth.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
cm_ameth.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
cm_ameth.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
cm_ameth.o: ../../include/openssl/opensslconf.h
cm_ameth.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cm_ameth.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
cm_ameth.o: ../../include/openssl/symhacks.h ../asn1/asn1_locl.h ../cryptlib.h
cm_ameth.o: cm_ameth.c
cm_pmeth.o: ../../e_os.h ../../include/openssl/asn1.h
cm_pmeth.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
cm_pmeth.o: ../../include/openssl/cmac.h ../../include/openssl/conf.h
cm_pmeth.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
cm_pmeth.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
cm_pmeth.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
cm_pmeth.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
cm_pmeth.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
cm_pmeth.o: ../../include/openssl/opensslconf.h
cm_pmeth.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cm_pmeth.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
cm_pmeth.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
cm_pmeth.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
cm_pmeth.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
cm_pmeth.o: ../cryptlib.h ../evp/evp_locl.h cm_pmeth.c
cmac.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
cmac.o: ../../include/openssl/buffer.h ../../include/openssl/cmac.h
cmac.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
cmac.o: ../../include/openssl/err.h ../../include/openssl/evp.h
cmac.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
cmac.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
cmac.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cmac.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
cmac.o: ../../include/openssl/symhacks.h ../cryptlib.h cmac.c
