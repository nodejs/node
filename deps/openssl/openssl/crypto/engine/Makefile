#
# OpenSSL/crypto/engine/Makefile
#

DIR=	engine
TOP=	../..
CC=	cc
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST= enginetest.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC= eng_err.c eng_lib.c eng_list.c eng_init.c eng_ctrl.c \
	eng_table.c eng_pkey.c eng_fat.c eng_all.c \
	tb_rsa.c tb_dsa.c tb_ecdsa.c tb_dh.c tb_ecdh.c tb_rand.c tb_store.c \
	tb_cipher.c tb_digest.c \
	eng_openssl.c eng_cnf.c eng_dyn.c eng_cryptodev.c eng_padlock.c
LIBOBJ= eng_err.o eng_lib.o eng_list.o eng_init.o eng_ctrl.o \
	eng_table.o eng_pkey.o eng_fat.o eng_all.o \
	tb_rsa.o tb_dsa.o tb_ecdsa.o tb_dh.o tb_ecdh.o tb_rand.o tb_store.o \
	tb_cipher.o tb_digest.o \
	eng_openssl.o eng_cnf.o eng_dyn.o eng_cryptodev.o eng_padlock.o

SRC= $(LIBSRC)

EXHEADER= engine.h
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

eng_all.o: ../../e_os.h ../../include/openssl/asn1.h
eng_all.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
eng_all.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
eng_all.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
eng_all.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
eng_all.o: ../../include/openssl/err.h ../../include/openssl/evp.h
eng_all.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
eng_all.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
eng_all.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
eng_all.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
eng_all.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
eng_all.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
eng_all.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
eng_all.o: ../cryptlib.h eng_all.c eng_int.h
eng_cnf.o: ../../e_os.h ../../include/openssl/asn1.h
eng_cnf.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
eng_cnf.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
eng_cnf.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
eng_cnf.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
eng_cnf.o: ../../include/openssl/engine.h ../../include/openssl/err.h
eng_cnf.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
eng_cnf.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
eng_cnf.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
eng_cnf.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
eng_cnf.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
eng_cnf.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_cnf.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_cnf.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_cnf.c eng_int.h
eng_cryptodev.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
eng_cryptodev.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
eng_cryptodev.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
eng_cryptodev.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
eng_cryptodev.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
eng_cryptodev.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
eng_cryptodev.o: ../../include/openssl/err.h ../../include/openssl/evp.h
eng_cryptodev.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
eng_cryptodev.o: ../../include/openssl/obj_mac.h
eng_cryptodev.o: ../../include/openssl/objects.h
eng_cryptodev.o: ../../include/openssl/opensslconf.h
eng_cryptodev.o: ../../include/openssl/opensslv.h
eng_cryptodev.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
eng_cryptodev.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
eng_cryptodev.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_cryptodev.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_cryptodev.o: ../../include/openssl/x509_vfy.h eng_cryptodev.c
eng_ctrl.o: ../../e_os.h ../../include/openssl/asn1.h
eng_ctrl.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
eng_ctrl.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
eng_ctrl.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
eng_ctrl.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
eng_ctrl.o: ../../include/openssl/err.h ../../include/openssl/evp.h
eng_ctrl.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
eng_ctrl.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
eng_ctrl.o: ../../include/openssl/opensslconf.h
eng_ctrl.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
eng_ctrl.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
eng_ctrl.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_ctrl.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_ctrl.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_ctrl.c eng_int.h
eng_dyn.o: ../../e_os.h ../../include/openssl/asn1.h
eng_dyn.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
eng_dyn.o: ../../include/openssl/crypto.h ../../include/openssl/dso.h
eng_dyn.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
eng_dyn.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
eng_dyn.o: ../../include/openssl/engine.h ../../include/openssl/err.h
eng_dyn.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
eng_dyn.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
eng_dyn.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
eng_dyn.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
eng_dyn.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
eng_dyn.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_dyn.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_dyn.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_dyn.c eng_int.h
eng_err.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
eng_err.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
eng_err.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
eng_err.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
eng_err.o: ../../include/openssl/engine.h ../../include/openssl/err.h
eng_err.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
eng_err.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
eng_err.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
eng_err.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
eng_err.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
eng_err.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_err.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_err.o: ../../include/openssl/x509_vfy.h eng_err.c
eng_fat.o: ../../e_os.h ../../include/openssl/asn1.h
eng_fat.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
eng_fat.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
eng_fat.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
eng_fat.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
eng_fat.o: ../../include/openssl/engine.h ../../include/openssl/err.h
eng_fat.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
eng_fat.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
eng_fat.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
eng_fat.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
eng_fat.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
eng_fat.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_fat.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_fat.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_fat.c eng_int.h
eng_init.o: ../../e_os.h ../../include/openssl/asn1.h
eng_init.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
eng_init.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
eng_init.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
eng_init.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
eng_init.o: ../../include/openssl/err.h ../../include/openssl/evp.h
eng_init.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
eng_init.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
eng_init.o: ../../include/openssl/opensslconf.h
eng_init.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
eng_init.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
eng_init.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_init.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_init.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_init.c eng_int.h
eng_lib.o: ../../e_os.h ../../include/openssl/asn1.h
eng_lib.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
eng_lib.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
eng_lib.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
eng_lib.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
eng_lib.o: ../../include/openssl/err.h ../../include/openssl/evp.h
eng_lib.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
eng_lib.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
eng_lib.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
eng_lib.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
eng_lib.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
eng_lib.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_lib.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_lib.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_int.h eng_lib.c
eng_list.o: ../../e_os.h ../../include/openssl/asn1.h
eng_list.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
eng_list.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
eng_list.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
eng_list.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
eng_list.o: ../../include/openssl/err.h ../../include/openssl/evp.h
eng_list.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
eng_list.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
eng_list.o: ../../include/openssl/opensslconf.h
eng_list.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
eng_list.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
eng_list.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_list.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_list.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_int.h eng_list.c
eng_openssl.o: ../../e_os.h ../../include/openssl/asn1.h
eng_openssl.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
eng_openssl.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
eng_openssl.o: ../../include/openssl/dsa.h ../../include/openssl/dso.h
eng_openssl.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
eng_openssl.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
eng_openssl.o: ../../include/openssl/engine.h ../../include/openssl/err.h
eng_openssl.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
eng_openssl.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
eng_openssl.o: ../../include/openssl/objects.h
eng_openssl.o: ../../include/openssl/opensslconf.h
eng_openssl.o: ../../include/openssl/opensslv.h
eng_openssl.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pem.h
eng_openssl.o: ../../include/openssl/pem2.h ../../include/openssl/pkcs7.h
eng_openssl.o: ../../include/openssl/rand.h ../../include/openssl/rc4.h
eng_openssl.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
eng_openssl.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_openssl.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_openssl.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_openssl.c
eng_padlock.o: ../../include/openssl/aes.h ../../include/openssl/asn1.h
eng_padlock.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
eng_padlock.o: ../../include/openssl/crypto.h ../../include/openssl/dso.h
eng_padlock.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
eng_padlock.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
eng_padlock.o: ../../include/openssl/engine.h ../../include/openssl/err.h
eng_padlock.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
eng_padlock.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
eng_padlock.o: ../../include/openssl/objects.h
eng_padlock.o: ../../include/openssl/opensslconf.h
eng_padlock.o: ../../include/openssl/opensslv.h
eng_padlock.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
eng_padlock.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
eng_padlock.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_padlock.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_padlock.o: ../../include/openssl/x509_vfy.h eng_padlock.c
eng_pkey.o: ../../e_os.h ../../include/openssl/asn1.h
eng_pkey.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
eng_pkey.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
eng_pkey.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
eng_pkey.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
eng_pkey.o: ../../include/openssl/err.h ../../include/openssl/evp.h
eng_pkey.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
eng_pkey.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
eng_pkey.o: ../../include/openssl/opensslconf.h
eng_pkey.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
eng_pkey.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
eng_pkey.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_pkey.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_pkey.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_int.h eng_pkey.c
eng_table.o: ../../e_os.h ../../include/openssl/asn1.h
eng_table.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
eng_table.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
eng_table.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
eng_table.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
eng_table.o: ../../include/openssl/err.h ../../include/openssl/evp.h
eng_table.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
eng_table.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
eng_table.o: ../../include/openssl/opensslconf.h
eng_table.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
eng_table.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
eng_table.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
eng_table.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
eng_table.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_int.h
eng_table.o: eng_table.c
tb_cipher.o: ../../e_os.h ../../include/openssl/asn1.h
tb_cipher.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
tb_cipher.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
tb_cipher.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
tb_cipher.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
tb_cipher.o: ../../include/openssl/err.h ../../include/openssl/evp.h
tb_cipher.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
tb_cipher.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
tb_cipher.o: ../../include/openssl/opensslconf.h
tb_cipher.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tb_cipher.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
tb_cipher.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
tb_cipher.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
tb_cipher.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_int.h
tb_cipher.o: tb_cipher.c
tb_dh.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
tb_dh.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
tb_dh.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
tb_dh.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
tb_dh.o: ../../include/openssl/engine.h ../../include/openssl/err.h
tb_dh.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
tb_dh.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
tb_dh.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
tb_dh.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tb_dh.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
tb_dh.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
tb_dh.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
tb_dh.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_int.h tb_dh.c
tb_digest.o: ../../e_os.h ../../include/openssl/asn1.h
tb_digest.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
tb_digest.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
tb_digest.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
tb_digest.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
tb_digest.o: ../../include/openssl/err.h ../../include/openssl/evp.h
tb_digest.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
tb_digest.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
tb_digest.o: ../../include/openssl/opensslconf.h
tb_digest.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tb_digest.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
tb_digest.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
tb_digest.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
tb_digest.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_int.h
tb_digest.o: tb_digest.c
tb_dsa.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
tb_dsa.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
tb_dsa.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
tb_dsa.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
tb_dsa.o: ../../include/openssl/engine.h ../../include/openssl/err.h
tb_dsa.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
tb_dsa.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
tb_dsa.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
tb_dsa.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tb_dsa.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
tb_dsa.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
tb_dsa.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
tb_dsa.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_int.h tb_dsa.c
tb_ecdh.o: ../../e_os.h ../../include/openssl/asn1.h
tb_ecdh.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
tb_ecdh.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
tb_ecdh.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
tb_ecdh.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
tb_ecdh.o: ../../include/openssl/err.h ../../include/openssl/evp.h
tb_ecdh.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
tb_ecdh.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
tb_ecdh.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
tb_ecdh.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
tb_ecdh.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
tb_ecdh.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
tb_ecdh.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
tb_ecdh.o: ../cryptlib.h eng_int.h tb_ecdh.c
tb_ecdsa.o: ../../e_os.h ../../include/openssl/asn1.h
tb_ecdsa.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
tb_ecdsa.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
tb_ecdsa.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
tb_ecdsa.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
tb_ecdsa.o: ../../include/openssl/err.h ../../include/openssl/evp.h
tb_ecdsa.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
tb_ecdsa.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
tb_ecdsa.o: ../../include/openssl/opensslconf.h
tb_ecdsa.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tb_ecdsa.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
tb_ecdsa.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
tb_ecdsa.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
tb_ecdsa.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_int.h tb_ecdsa.c
tb_rand.o: ../../e_os.h ../../include/openssl/asn1.h
tb_rand.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
tb_rand.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
tb_rand.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
tb_rand.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
tb_rand.o: ../../include/openssl/err.h ../../include/openssl/evp.h
tb_rand.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
tb_rand.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
tb_rand.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
tb_rand.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
tb_rand.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
tb_rand.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
tb_rand.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
tb_rand.o: ../cryptlib.h eng_int.h tb_rand.c
tb_rsa.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
tb_rsa.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
tb_rsa.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
tb_rsa.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
tb_rsa.o: ../../include/openssl/engine.h ../../include/openssl/err.h
tb_rsa.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
tb_rsa.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
tb_rsa.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
tb_rsa.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tb_rsa.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
tb_rsa.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
tb_rsa.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
tb_rsa.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_int.h tb_rsa.c
tb_store.o: ../../e_os.h ../../include/openssl/asn1.h
tb_store.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
tb_store.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
tb_store.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
tb_store.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
tb_store.o: ../../include/openssl/err.h ../../include/openssl/evp.h
tb_store.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
tb_store.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
tb_store.o: ../../include/openssl/opensslconf.h
tb_store.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tb_store.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
tb_store.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
tb_store.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
tb_store.o: ../../include/openssl/x509_vfy.h ../cryptlib.h eng_int.h tb_store.c
