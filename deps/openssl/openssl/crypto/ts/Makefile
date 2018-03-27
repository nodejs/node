#
# SSLeay/crypto/ts/Makefile
#

DIR=	ts
TOP=	../..
CC=	cc
INCLUDES= -I.. -I../../include
CFLAG = -g
INSTALL_PREFIX=
OPENSSLDIR=     /usr/local/ssl
INSTALLTOP=/usr/local/ssl
MAKEDEPPROG=	makedepend
MAKEDEPEND=	$(TOP)/util/domd $(TOP) -MD $(MAKEDEPPROG)
MAKEFILE=	Makefile
AR=		ar r

PEX_LIBS=
EX_LIBS=
 
CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL= Makefile
TEST=
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=	ts_err.c ts_req_utils.c ts_req_print.c ts_rsp_utils.c ts_rsp_print.c \
	ts_rsp_sign.c ts_rsp_verify.c ts_verify_ctx.c ts_lib.c ts_conf.c \
	ts_asn1.c
LIBOBJ= ts_err.o ts_req_utils.o ts_req_print.o ts_rsp_utils.o ts_rsp_print.o \
	ts_rsp_sign.o ts_rsp_verify.o ts_verify_ctx.o ts_lib.o ts_conf.o \
	ts_asn1.o

SRC= $(LIBSRC)

EXHEADER= ts.h
HEADER=	$(EXHEADER)

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

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

update: depend

depend:
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(LIBSRC)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff enc dec sign verify

# DO NOT DELETE THIS LINE -- make depend depends on it.

ts_asn1.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
ts_asn1.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
ts_asn1.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
ts_asn1.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
ts_asn1.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ts_asn1.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
ts_asn1.o: ../../include/openssl/err.h ../../include/openssl/evp.h
ts_asn1.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
ts_asn1.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
ts_asn1.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ts_asn1.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
ts_asn1.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
ts_asn1.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
ts_asn1.o: ../../include/openssl/ts.h ../../include/openssl/x509.h
ts_asn1.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
ts_asn1.o: ts_asn1.c
ts_conf.o: ../../e_os.h ../../include/openssl/asn1.h
ts_conf.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
ts_conf.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
ts_conf.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
ts_conf.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ts_conf.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
ts_conf.o: ../../include/openssl/engine.h ../../include/openssl/err.h
ts_conf.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
ts_conf.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
ts_conf.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
ts_conf.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pem.h
ts_conf.o: ../../include/openssl/pem2.h ../../include/openssl/pkcs7.h
ts_conf.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
ts_conf.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
ts_conf.o: ../../include/openssl/symhacks.h ../../include/openssl/ts.h
ts_conf.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
ts_conf.o: ../../include/openssl/x509v3.h ../cryptlib.h ts_conf.c
ts_err.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ts_err.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
ts_err.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
ts_err.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
ts_err.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
ts_err.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
ts_err.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
ts_err.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
ts_err.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
ts_err.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
ts_err.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
ts_err.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
ts_err.o: ../../include/openssl/symhacks.h ../../include/openssl/ts.h
ts_err.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
ts_err.o: ../../include/openssl/x509v3.h ts_err.c
ts_lib.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
ts_lib.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
ts_lib.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
ts_lib.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
ts_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ts_lib.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
ts_lib.o: ../../include/openssl/err.h ../../include/openssl/evp.h
ts_lib.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
ts_lib.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
ts_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ts_lib.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
ts_lib.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
ts_lib.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
ts_lib.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
ts_lib.o: ../../include/openssl/x509v3.h ../cryptlib.h ts.h ts_lib.c
ts_req_print.o: ../../e_os.h ../../include/openssl/asn1.h
ts_req_print.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
ts_req_print.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
ts_req_print.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
ts_req_print.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
ts_req_print.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
ts_req_print.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
ts_req_print.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
ts_req_print.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
ts_req_print.o: ../../include/openssl/opensslconf.h
ts_req_print.o: ../../include/openssl/opensslv.h
ts_req_print.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
ts_req_print.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
ts_req_print.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
ts_req_print.o: ../../include/openssl/symhacks.h ../../include/openssl/ts.h
ts_req_print.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
ts_req_print.o: ../../include/openssl/x509v3.h ../cryptlib.h ts_req_print.c
ts_req_utils.o: ../../e_os.h ../../include/openssl/asn1.h
ts_req_utils.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
ts_req_utils.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
ts_req_utils.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
ts_req_utils.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ts_req_utils.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
ts_req_utils.o: ../../include/openssl/err.h ../../include/openssl/evp.h
ts_req_utils.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
ts_req_utils.o: ../../include/openssl/objects.h
ts_req_utils.o: ../../include/openssl/opensslconf.h
ts_req_utils.o: ../../include/openssl/opensslv.h
ts_req_utils.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
ts_req_utils.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
ts_req_utils.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
ts_req_utils.o: ../../include/openssl/symhacks.h ../../include/openssl/ts.h
ts_req_utils.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
ts_req_utils.o: ../../include/openssl/x509v3.h ../cryptlib.h ts_req_utils.c
ts_rsp_print.o: ../../e_os.h ../../include/openssl/asn1.h
ts_rsp_print.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
ts_rsp_print.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
ts_rsp_print.o: ../../include/openssl/crypto.h ../../include/openssl/dh.h
ts_rsp_print.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
ts_rsp_print.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
ts_rsp_print.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
ts_rsp_print.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
ts_rsp_print.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
ts_rsp_print.o: ../../include/openssl/opensslconf.h
ts_rsp_print.o: ../../include/openssl/opensslv.h
ts_rsp_print.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
ts_rsp_print.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
ts_rsp_print.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
ts_rsp_print.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
ts_rsp_print.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
ts_rsp_print.o: ../cryptlib.h ts.h ts_rsp_print.c
ts_rsp_sign.o: ../../e_os.h ../../include/openssl/asn1.h
ts_rsp_sign.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
ts_rsp_sign.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
ts_rsp_sign.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
ts_rsp_sign.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ts_rsp_sign.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
ts_rsp_sign.o: ../../include/openssl/err.h ../../include/openssl/evp.h
ts_rsp_sign.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
ts_rsp_sign.o: ../../include/openssl/objects.h
ts_rsp_sign.o: ../../include/openssl/opensslconf.h
ts_rsp_sign.o: ../../include/openssl/opensslv.h
ts_rsp_sign.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
ts_rsp_sign.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
ts_rsp_sign.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
ts_rsp_sign.o: ../../include/openssl/symhacks.h ../../include/openssl/ts.h
ts_rsp_sign.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
ts_rsp_sign.o: ../../include/openssl/x509v3.h ../cryptlib.h ../o_time.h
ts_rsp_sign.o: ts_rsp_sign.c
ts_rsp_utils.o: ../../e_os.h ../../include/openssl/asn1.h
ts_rsp_utils.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
ts_rsp_utils.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
ts_rsp_utils.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
ts_rsp_utils.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ts_rsp_utils.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
ts_rsp_utils.o: ../../include/openssl/err.h ../../include/openssl/evp.h
ts_rsp_utils.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
ts_rsp_utils.o: ../../include/openssl/objects.h
ts_rsp_utils.o: ../../include/openssl/opensslconf.h
ts_rsp_utils.o: ../../include/openssl/opensslv.h
ts_rsp_utils.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
ts_rsp_utils.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
ts_rsp_utils.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
ts_rsp_utils.o: ../../include/openssl/symhacks.h ../../include/openssl/ts.h
ts_rsp_utils.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
ts_rsp_utils.o: ../../include/openssl/x509v3.h ../cryptlib.h ts_rsp_utils.c
ts_rsp_verify.o: ../../e_os.h ../../include/openssl/asn1.h
ts_rsp_verify.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
ts_rsp_verify.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
ts_rsp_verify.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
ts_rsp_verify.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ts_rsp_verify.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
ts_rsp_verify.o: ../../include/openssl/err.h ../../include/openssl/evp.h
ts_rsp_verify.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
ts_rsp_verify.o: ../../include/openssl/objects.h
ts_rsp_verify.o: ../../include/openssl/opensslconf.h
ts_rsp_verify.o: ../../include/openssl/opensslv.h
ts_rsp_verify.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
ts_rsp_verify.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
ts_rsp_verify.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
ts_rsp_verify.o: ../../include/openssl/symhacks.h ../../include/openssl/ts.h
ts_rsp_verify.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
ts_rsp_verify.o: ../../include/openssl/x509v3.h ../cryptlib.h ts_rsp_verify.c
ts_verify_ctx.o: ../../e_os.h ../../include/openssl/asn1.h
ts_verify_ctx.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
ts_verify_ctx.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
ts_verify_ctx.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
ts_verify_ctx.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ts_verify_ctx.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
ts_verify_ctx.o: ../../include/openssl/err.h ../../include/openssl/evp.h
ts_verify_ctx.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
ts_verify_ctx.o: ../../include/openssl/objects.h
ts_verify_ctx.o: ../../include/openssl/opensslconf.h
ts_verify_ctx.o: ../../include/openssl/opensslv.h
ts_verify_ctx.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
ts_verify_ctx.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
ts_verify_ctx.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
ts_verify_ctx.o: ../../include/openssl/symhacks.h ../../include/openssl/ts.h
ts_verify_ctx.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
ts_verify_ctx.o: ../../include/openssl/x509v3.h ../cryptlib.h ts_verify_ctx.c
