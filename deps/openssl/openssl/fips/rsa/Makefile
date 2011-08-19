#
# OpenSSL/fips/rsa/Makefile
#

DIR=	rsa
TOP=	../..
CC=	cc
INCLUDES=
CFLAG=-g
INSTALL_PREFIX=
OPENSSLDIR=     /usr/local/ssl
INSTALLTOP=/usr/local/ssl
MAKEDEPPROG=	makedepend
MAKEDEPEND=	$(TOP)/util/domd $(TOP) -MD $(MAKEDEPPROG)
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST= fips_rsavtest.c fips_rsastest.c fips_rsagtest.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=fips_rsa_eay.c fips_rsa_gen.c fips_rsa_selftest.c fips_rsa_x931g.c \
	fips_rsa_sign.c fips_rsa_lib.c
LIBOBJ=fips_rsa_eay.o fips_rsa_gen.o fips_rsa_selftest.o fips_rsa_x931g.o \
	fips_rsa_sign.o fips_rsa_lib.o

SRC= $(LIBSRC)

EXHEADER=
HEADER=	$(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd $(TOP); $(MAKE) DIRS=fips FDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	@echo $(LIBOBJ) > lib

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO

links:
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/include/openssl $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/test $(TEST)
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/apps $(APPS)

install:
	@headerlist="$(EXHEADER)"; for i in $$headerlist; \
	do \
	  (cp $$i $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i; \
	  chmod 644 $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i ); \
	done

tags:
	ctags $(SRC)

tests:

Q=../testvectors/rsa/req
A=../testvectors/rsa/rsp
Q62=../testvectors/rsa_salt_62/req
A62=../testvectors/rsa_salt_62/rsp

fips_test:
	-rm -rf $(A) $(A62)
	mkdir $(A) $(A62)
	if [ -f $(Q)/SigGen15.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rsastest < $(Q)/SigGen15.req  > $(A)/SigGen15.rsp; fi
	if [ -f $(Q)/SigVer15.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rsavtest < $(Q)/SigVer15.req > $(A)/SigVer15.rsp; fi
	if [ -f $(Q)/SigGenPSS.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rsastest -saltlen 0 < $(Q)/SigGenPSS.req > $(A)/SigGenPSS.rsp; fi
	if [ -f $(Q)/SigVerPSS.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rsavtest -saltlen 0 < $(Q)/SigVerPSS.req > $(A)/SigVerPSS.rsp; fi
	if [ -f $(Q)/SigGenRSA.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rsastest -x931 < $(Q)/SigGenRSA.req > $(A)/SigGenRSA.rsp; fi
	if [ -f $(Q)/SigVerRSA.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rsavtest -x931 < $(Q)/SigVerRSA.req > $(A)/SigVerRSA.rsp; fi
	if [ -f $(Q62)/SigGenPSS.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rsastest -saltlen 62 < $(Q62)/SigGenPSS.req >$(A62)/SigGenPSS.rsp; fi
	if [ -f $(Q62)/SigVerPSS.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rsavtest -saltlen 62 <$(Q62)/SigVerPSS.req >$(A62)/SigVerPSS.rsp; fi
	if [ -f $(Q)/KeyGenRSA.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rsagtest < $(Q)/KeyGenRSA.req > $(A)/KeyGenRSA.rsp; fi

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

depend:
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(SRC) $(TEST)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff
# DO NOT DELETE THIS LINE -- make depend depends on it.

fips_rsa_eay.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_rsa_eay.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
fips_rsa_eay.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_rsa_eay.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
fips_rsa_eay.o: ../../include/openssl/opensslconf.h
fips_rsa_eay.o: ../../include/openssl/opensslv.h
fips_rsa_eay.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rand.h
fips_rsa_eay.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
fips_rsa_eay.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
fips_rsa_eay.o: fips_rsa_eay.c
fips_rsa_gen.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_rsa_gen.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
fips_rsa_gen.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_rsa_gen.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
fips_rsa_gen.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
fips_rsa_gen.o: ../../include/openssl/objects.h
fips_rsa_gen.o: ../../include/openssl/opensslconf.h
fips_rsa_gen.o: ../../include/openssl/opensslv.h
fips_rsa_gen.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rsa.h
fips_rsa_gen.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
fips_rsa_gen.o: ../../include/openssl/symhacks.h ../fips_locl.h fips_rsa_gen.c
fips_rsa_lib.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_rsa_lib.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
fips_rsa_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_rsa_lib.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
fips_rsa_lib.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
fips_rsa_lib.o: ../../include/openssl/objects.h
fips_rsa_lib.o: ../../include/openssl/opensslconf.h
fips_rsa_lib.o: ../../include/openssl/opensslv.h
fips_rsa_lib.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rsa.h
fips_rsa_lib.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
fips_rsa_lib.o: ../../include/openssl/symhacks.h fips_rsa_lib.c
fips_rsa_selftest.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_rsa_selftest.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
fips_rsa_selftest.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_rsa_selftest.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
fips_rsa_selftest.o: ../../include/openssl/lhash.h
fips_rsa_selftest.o: ../../include/openssl/obj_mac.h
fips_rsa_selftest.o: ../../include/openssl/objects.h
fips_rsa_selftest.o: ../../include/openssl/opensslconf.h
fips_rsa_selftest.o: ../../include/openssl/opensslv.h
fips_rsa_selftest.o: ../../include/openssl/ossl_typ.h
fips_rsa_selftest.o: ../../include/openssl/rsa.h
fips_rsa_selftest.o: ../../include/openssl/safestack.h
fips_rsa_selftest.o: ../../include/openssl/stack.h
fips_rsa_selftest.o: ../../include/openssl/symhacks.h fips_rsa_selftest.c
fips_rsa_sign.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_rsa_sign.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
fips_rsa_sign.o: ../../include/openssl/err.h ../../include/openssl/evp.h
fips_rsa_sign.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
fips_rsa_sign.o: ../../include/openssl/obj_mac.h
fips_rsa_sign.o: ../../include/openssl/objects.h
fips_rsa_sign.o: ../../include/openssl/opensslconf.h
fips_rsa_sign.o: ../../include/openssl/opensslv.h
fips_rsa_sign.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rsa.h
fips_rsa_sign.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
fips_rsa_sign.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
fips_rsa_sign.o: fips_rsa_sign.c
fips_rsa_x931g.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_rsa_x931g.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
fips_rsa_x931g.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_rsa_x931g.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
fips_rsa_x931g.o: ../../include/openssl/opensslconf.h
fips_rsa_x931g.o: ../../include/openssl/opensslv.h
fips_rsa_x931g.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rsa.h
fips_rsa_x931g.o: ../../include/openssl/safestack.h
fips_rsa_x931g.o: ../../include/openssl/stack.h
fips_rsa_x931g.o: ../../include/openssl/symhacks.h fips_rsa_x931g.c
fips_rsagtest.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_rsagtest.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
fips_rsagtest.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
fips_rsagtest.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
fips_rsagtest.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
fips_rsagtest.o: ../../include/openssl/err.h ../../include/openssl/evp.h
fips_rsagtest.o: ../../include/openssl/fips.h ../../include/openssl/hmac.h
fips_rsagtest.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
fips_rsagtest.o: ../../include/openssl/objects.h
fips_rsagtest.o: ../../include/openssl/opensslconf.h
fips_rsagtest.o: ../../include/openssl/opensslv.h
fips_rsagtest.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
fips_rsagtest.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
fips_rsagtest.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
fips_rsagtest.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
fips_rsagtest.o: ../../include/openssl/x509_vfy.h
fips_rsagtest.o: ../../include/openssl/x509v3.h ../fips_utl.h fips_rsagtest.c
fips_rsastest.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_rsastest.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
fips_rsastest.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
fips_rsastest.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
fips_rsastest.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
fips_rsastest.o: ../../include/openssl/err.h ../../include/openssl/evp.h
fips_rsastest.o: ../../include/openssl/fips.h ../../include/openssl/hmac.h
fips_rsastest.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
fips_rsastest.o: ../../include/openssl/objects.h
fips_rsastest.o: ../../include/openssl/opensslconf.h
fips_rsastest.o: ../../include/openssl/opensslv.h
fips_rsastest.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
fips_rsastest.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
fips_rsastest.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
fips_rsastest.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
fips_rsastest.o: ../../include/openssl/x509_vfy.h
fips_rsastest.o: ../../include/openssl/x509v3.h ../fips_utl.h fips_rsastest.c
fips_rsavtest.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_rsavtest.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
fips_rsavtest.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
fips_rsavtest.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
fips_rsavtest.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
fips_rsavtest.o: ../../include/openssl/err.h ../../include/openssl/evp.h
fips_rsavtest.o: ../../include/openssl/fips.h ../../include/openssl/hmac.h
fips_rsavtest.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
fips_rsavtest.o: ../../include/openssl/objects.h
fips_rsavtest.o: ../../include/openssl/opensslconf.h
fips_rsavtest.o: ../../include/openssl/opensslv.h
fips_rsavtest.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
fips_rsavtest.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
fips_rsavtest.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
fips_rsavtest.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
fips_rsavtest.o: ../../include/openssl/x509_vfy.h
fips_rsavtest.o: ../../include/openssl/x509v3.h ../fips_utl.h fips_rsavtest.c
