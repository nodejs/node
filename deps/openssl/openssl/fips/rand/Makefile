#
# OpenSSL/fips/rand/Makefile
#

DIR=	rand
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
TEST= fips_randtest.c fips_rngvs.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=fips_rand.c fips_rand_selftest.c
LIBOBJ=fips_rand.o fips_rand_selftest.o

SRC= $(LIBSRC)

EXHEADER= fips_rand.h
HEADER=	$(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd $(TOP); $(MAKE) DIRS=fips SDIRS=$(DIR) sub_all)

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

Q=../testvectors/rng/req
A=../testvectors/rng/rsp

fips_test:
	-rm -rf $(A)
	mkdir $(A)
	if [ -f $(Q)/ANSI931_AES128MCT.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rngvs mct < $(Q)/ANSI931_AES128MCT.req > $(A)/ANSI931_AES128MCT.rsp; fi
	if [ -f $(Q)/ANSI931_AES192MCT.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rngvs mct < $(Q)/ANSI931_AES192MCT.req > $(A)/ANSI931_AES192MCT.rsp; fi
	if [ -f $(Q)/ANSI931_AES256MCT.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rngvs mct < $(Q)/ANSI931_AES256MCT.req > $(A)/ANSI931_AES256MCT.rsp; fi
	if [ -f $(Q)/ANSI931_AES128VST.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rngvs vst < $(Q)/ANSI931_AES128VST.req > $(A)/ANSI931_AES128VST.rsp; fi
	if [ -f $(Q)/ANSI931_AES192VST.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rngvs vst < $(Q)/ANSI931_AES192VST.req > $(A)/ANSI931_AES192VST.rsp; fi
	if [ -f $(Q)/ANSI931_AES256VST.req ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_rngvs vst < $(Q)/ANSI931_AES256VST.req > $(A)/ANSI931_AES256VST.rsp; fi

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

fips_rand.o: ../../e_os.h ../../include/openssl/aes.h
fips_rand.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
fips_rand.o: ../../include/openssl/des.h ../../include/openssl/des_old.h
fips_rand.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_rand.o: ../../include/openssl/fips.h ../../include/openssl/fips_rand.h
fips_rand.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
fips_rand.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
fips_rand.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
fips_rand.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
fips_rand.o: ../../include/openssl/ui.h ../../include/openssl/ui_compat.h
fips_rand.o: ../fips_locl.h fips_rand.c
fips_rand_selftest.o: ../../include/openssl/bio.h
fips_rand_selftest.o: ../../include/openssl/crypto.h
fips_rand_selftest.o: ../../include/openssl/des.h
fips_rand_selftest.o: ../../include/openssl/des_old.h
fips_rand_selftest.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_rand_selftest.o: ../../include/openssl/fips.h
fips_rand_selftest.o: ../../include/openssl/fips_rand.h
fips_rand_selftest.o: ../../include/openssl/lhash.h
fips_rand_selftest.o: ../../include/openssl/opensslconf.h
fips_rand_selftest.o: ../../include/openssl/opensslv.h
fips_rand_selftest.o: ../../include/openssl/ossl_typ.h
fips_rand_selftest.o: ../../include/openssl/rand.h
fips_rand_selftest.o: ../../include/openssl/safestack.h
fips_rand_selftest.o: ../../include/openssl/stack.h
fips_rand_selftest.o: ../../include/openssl/symhacks.h
fips_rand_selftest.o: ../../include/openssl/ui.h
fips_rand_selftest.o: ../../include/openssl/ui_compat.h fips_rand_selftest.c
fips_randtest.o: ../../e_os.h ../../include/openssl/bio.h
fips_randtest.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
fips_randtest.o: ../../include/openssl/des.h ../../include/openssl/des_old.h
fips_randtest.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_randtest.o: ../../include/openssl/fips_rand.h
fips_randtest.o: ../../include/openssl/lhash.h
fips_randtest.o: ../../include/openssl/opensslconf.h
fips_randtest.o: ../../include/openssl/opensslv.h
fips_randtest.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rand.h
fips_randtest.o: ../../include/openssl/safestack.h
fips_randtest.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
fips_randtest.o: ../../include/openssl/ui.h ../../include/openssl/ui_compat.h
fips_randtest.o: ../fips_utl.h fips_randtest.c
fips_rngvs.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_rngvs.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
fips_rngvs.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
fips_rngvs.o: ../../include/openssl/des.h ../../include/openssl/des_old.h
fips_rngvs.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
fips_rngvs.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
fips_rngvs.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
fips_rngvs.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
fips_rngvs.o: ../../include/openssl/fips_rand.h ../../include/openssl/lhash.h
fips_rngvs.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
fips_rngvs.o: ../../include/openssl/opensslconf.h
fips_rngvs.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
fips_rngvs.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
fips_rngvs.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
fips_rngvs.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
fips_rngvs.o: ../../include/openssl/ui.h ../../include/openssl/ui_compat.h
fips_rngvs.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
fips_rngvs.o: ../../include/openssl/x509v3.h ../fips_utl.h fips_rngvs.c
