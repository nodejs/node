#
# OpenSSL/crypto/err/Makefile
#

DIR=	err
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
LIBSRC=err.c err_all.c err_prn.c
LIBOBJ=err.o err_all.o err_prn.o

SRC= $(LIBSRC)

EXHEADER= err.h
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

err.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/buffer.h
err.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
err.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
err.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
err.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
err.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
err.o: ../cryptlib.h err.c
err_all.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
err_all.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
err_all.o: ../../include/openssl/cms.h ../../include/openssl/comp.h
err_all.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
err_all.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
err_all.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
err_all.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
err_all.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
err_all.o: ../../include/openssl/err.h ../../include/openssl/evp.h
err_all.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
err_all.o: ../../include/openssl/objects.h ../../include/openssl/ocsp.h
err_all.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
err_all.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pem2.h
err_all.o: ../../include/openssl/pkcs12.h ../../include/openssl/pkcs7.h
err_all.o: ../../include/openssl/rand.h ../../include/openssl/rsa.h
err_all.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
err_all.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
err_all.o: ../../include/openssl/ts.h ../../include/openssl/ui.h
err_all.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
err_all.o: ../../include/openssl/x509v3.h err_all.c
err_prn.o: ../../e_os.h ../../include/openssl/bio.h
err_prn.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
err_prn.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
err_prn.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
err_prn.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
err_prn.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
err_prn.o: ../../include/openssl/symhacks.h ../cryptlib.h err_prn.c
