#
# OpenSSL/crypto/asn1/Makefile
#

DIR=	asn1
TOP=	../..
CC=	cc
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile README
TEST=
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=	a_object.c a_bitstr.c a_utctm.c a_gentm.c a_time.c a_int.c a_octet.c \
	a_print.c a_type.c a_set.c a_dup.c a_d2i_fp.c a_i2d_fp.c \
	a_enum.c a_utf8.c a_sign.c a_digest.c a_verify.c a_mbstr.c a_strex.c \
	x_algor.c x_val.c x_pubkey.c x_sig.c x_req.c x_attrib.c x_bignum.c \
	x_long.c x_name.c x_x509.c x_x509a.c x_crl.c x_info.c x_spki.c nsseq.c \
	x_nx509.c d2i_pu.c d2i_pr.c i2d_pu.c i2d_pr.c\
	t_req.c t_x509.c t_x509a.c t_crl.c t_pkey.c t_spki.c t_bitst.c \
	tasn_new.c tasn_fre.c tasn_enc.c tasn_dec.c tasn_utl.c tasn_typ.c \
	tasn_prn.c ameth_lib.c \
	f_int.c f_string.c n_pkey.c \
	f_enum.c x_pkey.c a_bool.c x_exten.c bio_asn1.c bio_ndef.c asn_mime.c \
	asn1_gen.c asn1_par.c asn1_lib.c asn1_err.c a_bytes.c a_strnid.c \
	evp_asn1.c asn_pack.c p5_pbe.c p5_pbev2.c p8_pkey.c asn_moid.c
LIBOBJ= a_object.o a_bitstr.o a_utctm.o a_gentm.o a_time.o a_int.o a_octet.o \
	a_print.o a_type.o a_set.o a_dup.o a_d2i_fp.o a_i2d_fp.o \
	a_enum.o a_utf8.o a_sign.o a_digest.o a_verify.o a_mbstr.o a_strex.o \
	x_algor.o x_val.o x_pubkey.o x_sig.o x_req.o x_attrib.o x_bignum.o \
	x_long.o x_name.o x_x509.o x_x509a.o x_crl.o x_info.o x_spki.o nsseq.o \
	x_nx509.o d2i_pu.o d2i_pr.o i2d_pu.o i2d_pr.o \
	t_req.o t_x509.o t_x509a.o t_crl.o t_pkey.o t_spki.o t_bitst.o \
	tasn_new.o tasn_fre.o tasn_enc.o tasn_dec.o tasn_utl.o tasn_typ.o \
	tasn_prn.o ameth_lib.o \
	f_int.o f_string.o n_pkey.o \
	f_enum.o x_pkey.o a_bool.o x_exten.o bio_asn1.o bio_ndef.o asn_mime.o \
	asn1_gen.o asn1_par.o asn1_lib.o asn1_err.o a_bytes.o a_strnid.o \
	evp_asn1.o asn_pack.o p5_pbe.o p5_pbev2.o p8_pkey.o asn_moid.o

SRC= $(LIBSRC)

EXHEADER=  asn1.h asn1_mac.h asn1t.h
HEADER=	$(EXHEADER) asn1_locl.h

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

test:	test.c
	cc -g -I../../include -c test.c
	cc -g -I../../include -o test test.o -L../.. -lcrypto

pk:	pk.c
	cc -g -I../../include -c pk.c
	cc -g -I../../include -o pk pk.o -L../.. -lcrypto

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
	@[ -n "$(MAKEDEPEND)" ] # should be set by top Makefile...
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(PROGS) $(LIBSRC)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff


# DO NOT DELETE THIS LINE -- make depend depends on it.

a_bitstr.o: ../../e_os.h ../../include/openssl/asn1.h
a_bitstr.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
a_bitstr.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_bitstr.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
a_bitstr.o: ../../include/openssl/opensslconf.h
a_bitstr.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_bitstr.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
a_bitstr.o: ../../include/openssl/symhacks.h ../cryptlib.h a_bitstr.c
a_bool.o: ../../e_os.h ../../include/openssl/asn1.h
a_bool.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
a_bool.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
a_bool.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
a_bool.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
a_bool.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_bool.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
a_bool.o: ../../include/openssl/symhacks.h ../cryptlib.h a_bool.c
a_bytes.o: ../../e_os.h ../../include/openssl/asn1.h
a_bytes.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
a_bytes.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_bytes.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
a_bytes.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
a_bytes.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
a_bytes.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
a_bytes.o: ../cryptlib.h a_bytes.c
a_d2i_fp.o: ../../e_os.h ../../include/openssl/asn1.h
a_d2i_fp.o: ../../include/openssl/asn1_mac.h ../../include/openssl/bio.h
a_d2i_fp.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
a_d2i_fp.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
a_d2i_fp.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
a_d2i_fp.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_d2i_fp.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
a_d2i_fp.o: ../../include/openssl/symhacks.h ../cryptlib.h a_d2i_fp.c
a_digest.o: ../../e_os.h ../../include/openssl/asn1.h
a_digest.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
a_digest.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_digest.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
a_digest.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
a_digest.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
a_digest.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
a_digest.o: ../../include/openssl/opensslconf.h
a_digest.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_digest.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
a_digest.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
a_digest.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
a_digest.o: ../../include/openssl/x509_vfy.h ../cryptlib.h a_digest.c
a_dup.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
a_dup.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
a_dup.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
a_dup.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
a_dup.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_dup.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
a_dup.o: ../../include/openssl/symhacks.h ../cryptlib.h a_dup.c
a_enum.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
a_enum.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
a_enum.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_enum.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
a_enum.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
a_enum.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
a_enum.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
a_enum.o: ../cryptlib.h a_enum.c
a_gentm.o: ../../e_os.h ../../include/openssl/asn1.h
a_gentm.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
a_gentm.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_gentm.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
a_gentm.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
a_gentm.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
a_gentm.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
a_gentm.o: ../cryptlib.h ../o_time.h a_gentm.c
a_i2d_fp.o: ../../e_os.h ../../include/openssl/asn1.h
a_i2d_fp.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
a_i2d_fp.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_i2d_fp.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
a_i2d_fp.o: ../../include/openssl/opensslconf.h
a_i2d_fp.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_i2d_fp.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
a_i2d_fp.o: ../../include/openssl/symhacks.h ../cryptlib.h a_i2d_fp.c
a_int.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
a_int.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
a_int.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_int.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
a_int.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
a_int.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
a_int.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
a_int.o: ../cryptlib.h a_int.c
a_mbstr.o: ../../e_os.h ../../include/openssl/asn1.h
a_mbstr.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
a_mbstr.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_mbstr.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
a_mbstr.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
a_mbstr.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
a_mbstr.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
a_mbstr.o: ../cryptlib.h a_mbstr.c
a_object.o: ../../e_os.h ../../include/openssl/asn1.h
a_object.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
a_object.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
a_object.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
a_object.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
a_object.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
a_object.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_object.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
a_object.o: ../../include/openssl/symhacks.h ../cryptlib.h a_object.c
a_octet.o: ../../e_os.h ../../include/openssl/asn1.h
a_octet.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
a_octet.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_octet.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
a_octet.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
a_octet.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
a_octet.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
a_octet.o: ../cryptlib.h a_octet.c
a_print.o: ../../e_os.h ../../include/openssl/asn1.h
a_print.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
a_print.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_print.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
a_print.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
a_print.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
a_print.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
a_print.o: ../cryptlib.h a_print.c
a_set.o: ../../e_os.h ../../include/openssl/asn1.h
a_set.o: ../../include/openssl/asn1_mac.h ../../include/openssl/bio.h
a_set.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
a_set.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
a_set.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
a_set.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_set.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
a_set.o: ../../include/openssl/symhacks.h ../cryptlib.h a_set.c
a_sign.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
a_sign.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
a_sign.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_sign.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
a_sign.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
a_sign.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
a_sign.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
a_sign.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
a_sign.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
a_sign.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
a_sign.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
a_sign.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
a_sign.o: ../cryptlib.h a_sign.c asn1_locl.h
a_strex.o: ../../e_os.h ../../include/openssl/asn1.h
a_strex.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
a_strex.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_strex.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
a_strex.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
a_strex.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
a_strex.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
a_strex.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
a_strex.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
a_strex.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
a_strex.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
a_strex.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
a_strex.o: ../cryptlib.h a_strex.c charmap.h
a_strnid.o: ../../e_os.h ../../include/openssl/asn1.h
a_strnid.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
a_strnid.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_strnid.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
a_strnid.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
a_strnid.o: ../../include/openssl/opensslconf.h
a_strnid.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_strnid.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
a_strnid.o: ../../include/openssl/symhacks.h ../cryptlib.h a_strnid.c
a_time.o: ../../e_os.h ../../include/openssl/asn1.h
a_time.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
a_time.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
a_time.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
a_time.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
a_time.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_time.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
a_time.o: ../../include/openssl/symhacks.h ../cryptlib.h ../o_time.h a_time.c
a_type.o: ../../e_os.h ../../include/openssl/asn1.h
a_type.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
a_type.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
a_type.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
a_type.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
a_type.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
a_type.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_type.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
a_type.o: ../../include/openssl/symhacks.h ../cryptlib.h a_type.c
a_utctm.o: ../../e_os.h ../../include/openssl/asn1.h
a_utctm.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
a_utctm.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
a_utctm.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
a_utctm.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
a_utctm.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
a_utctm.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
a_utctm.o: ../cryptlib.h ../o_time.h a_utctm.c
a_utf8.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
a_utf8.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
a_utf8.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
a_utf8.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
a_utf8.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_utf8.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
a_utf8.o: ../../include/openssl/symhacks.h ../cryptlib.h a_utf8.c
a_verify.o: ../../e_os.h ../../include/openssl/asn1.h
a_verify.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
a_verify.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
a_verify.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
a_verify.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
a_verify.o: ../../include/openssl/err.h ../../include/openssl/evp.h
a_verify.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
a_verify.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
a_verify.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
a_verify.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
a_verify.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
a_verify.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
a_verify.o: ../../include/openssl/x509_vfy.h ../cryptlib.h a_verify.c
a_verify.o: asn1_locl.h
ameth_lib.o: ../../e_os.h ../../include/openssl/asn1.h
ameth_lib.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
ameth_lib.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
ameth_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ameth_lib.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
ameth_lib.o: ../../include/openssl/engine.h ../../include/openssl/err.h
ameth_lib.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
ameth_lib.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
ameth_lib.o: ../../include/openssl/opensslconf.h
ameth_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ameth_lib.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
ameth_lib.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
ameth_lib.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
ameth_lib.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ameth_lib.c
ameth_lib.o: asn1_locl.h
asn1_err.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
asn1_err.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
asn1_err.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
asn1_err.o: ../../include/openssl/opensslconf.h
asn1_err.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
asn1_err.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
asn1_err.o: ../../include/openssl/symhacks.h asn1_err.c
asn1_gen.o: ../../e_os.h ../../include/openssl/asn1.h
asn1_gen.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
asn1_gen.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
asn1_gen.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
asn1_gen.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
asn1_gen.o: ../../include/openssl/err.h ../../include/openssl/evp.h
asn1_gen.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
asn1_gen.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
asn1_gen.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
asn1_gen.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
asn1_gen.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
asn1_gen.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
asn1_gen.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
asn1_gen.o: ../cryptlib.h asn1_gen.c
asn1_lib.o: ../../e_os.h ../../include/openssl/asn1.h
asn1_lib.o: ../../include/openssl/asn1_mac.h ../../include/openssl/bio.h
asn1_lib.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
asn1_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
asn1_lib.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
asn1_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
asn1_lib.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
asn1_lib.o: ../../include/openssl/symhacks.h ../cryptlib.h asn1_lib.c
asn1_par.o: ../../e_os.h ../../include/openssl/asn1.h
asn1_par.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
asn1_par.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
asn1_par.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
asn1_par.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
asn1_par.o: ../../include/openssl/opensslconf.h
asn1_par.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
asn1_par.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
asn1_par.o: ../../include/openssl/symhacks.h ../cryptlib.h asn1_par.c
asn_mime.o: ../../e_os.h ../../include/openssl/asn1.h
asn_mime.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
asn_mime.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
asn_mime.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
asn_mime.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
asn_mime.o: ../../include/openssl/err.h ../../include/openssl/evp.h
asn_mime.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
asn_mime.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
asn_mime.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
asn_mime.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
asn_mime.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
asn_mime.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
asn_mime.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
asn_mime.o: ../cryptlib.h asn1_locl.h asn_mime.c
asn_moid.o: ../../e_os.h ../../include/openssl/asn1.h
asn_moid.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
asn_moid.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
asn_moid.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
asn_moid.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
asn_moid.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
asn_moid.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
asn_moid.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
asn_moid.o: ../../include/openssl/opensslconf.h
asn_moid.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
asn_moid.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
asn_moid.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
asn_moid.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
asn_moid.o: ../../include/openssl/x509_vfy.h ../cryptlib.h asn_moid.c
asn_pack.o: ../../e_os.h ../../include/openssl/asn1.h
asn_pack.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
asn_pack.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
asn_pack.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
asn_pack.o: ../../include/openssl/opensslconf.h
asn_pack.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
asn_pack.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
asn_pack.o: ../../include/openssl/symhacks.h ../cryptlib.h asn_pack.c
bio_asn1.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
bio_asn1.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
bio_asn1.o: ../../include/openssl/opensslconf.h
bio_asn1.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bio_asn1.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bio_asn1.o: ../../include/openssl/symhacks.h bio_asn1.c
bio_ndef.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
bio_ndef.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
bio_ndef.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bio_ndef.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bio_ndef.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bio_ndef.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bio_ndef.o: ../../include/openssl/symhacks.h bio_ndef.c
d2i_pr.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
d2i_pr.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
d2i_pr.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
d2i_pr.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
d2i_pr.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
d2i_pr.o: ../../include/openssl/err.h ../../include/openssl/evp.h
d2i_pr.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
d2i_pr.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
d2i_pr.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
d2i_pr.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
d2i_pr.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
d2i_pr.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
d2i_pr.o: ../../include/openssl/x509_vfy.h ../cryptlib.h asn1_locl.h d2i_pr.c
d2i_pu.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
d2i_pu.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
d2i_pu.o: ../../include/openssl/crypto.h ../../include/openssl/dsa.h
d2i_pu.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
d2i_pu.o: ../../include/openssl/err.h ../../include/openssl/evp.h
d2i_pu.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
d2i_pu.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
d2i_pu.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
d2i_pu.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
d2i_pu.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
d2i_pu.o: ../cryptlib.h d2i_pu.c
evp_asn1.o: ../../e_os.h ../../include/openssl/asn1.h
evp_asn1.o: ../../include/openssl/asn1_mac.h ../../include/openssl/bio.h
evp_asn1.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
evp_asn1.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
evp_asn1.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
evp_asn1.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
evp_asn1.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
evp_asn1.o: ../../include/openssl/symhacks.h ../cryptlib.h evp_asn1.c
f_enum.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
f_enum.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
f_enum.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
f_enum.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
f_enum.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
f_enum.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
f_enum.o: ../../include/openssl/symhacks.h ../cryptlib.h f_enum.c
f_int.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
f_int.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
f_int.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
f_int.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
f_int.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
f_int.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
f_int.o: ../../include/openssl/symhacks.h ../cryptlib.h f_int.c
f_string.o: ../../e_os.h ../../include/openssl/asn1.h
f_string.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
f_string.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
f_string.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
f_string.o: ../../include/openssl/opensslconf.h
f_string.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
f_string.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
f_string.o: ../../include/openssl/symhacks.h ../cryptlib.h f_string.c
i2d_pr.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
i2d_pr.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
i2d_pr.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
i2d_pr.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
i2d_pr.o: ../../include/openssl/err.h ../../include/openssl/evp.h
i2d_pr.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
i2d_pr.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
i2d_pr.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
i2d_pr.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
i2d_pr.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
i2d_pr.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
i2d_pr.o: ../../include/openssl/x509_vfy.h ../cryptlib.h asn1_locl.h i2d_pr.c
i2d_pu.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
i2d_pu.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
i2d_pu.o: ../../include/openssl/crypto.h ../../include/openssl/dsa.h
i2d_pu.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
i2d_pu.o: ../../include/openssl/err.h ../../include/openssl/evp.h
i2d_pu.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
i2d_pu.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
i2d_pu.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
i2d_pu.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
i2d_pu.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
i2d_pu.o: ../cryptlib.h i2d_pu.c
n_pkey.o: ../../e_os.h ../../include/openssl/asn1.h
n_pkey.o: ../../include/openssl/asn1_mac.h ../../include/openssl/asn1t.h
n_pkey.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
n_pkey.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
n_pkey.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
n_pkey.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
n_pkey.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
n_pkey.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
n_pkey.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
n_pkey.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
n_pkey.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
n_pkey.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
n_pkey.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
n_pkey.o: ../../include/openssl/x509_vfy.h ../cryptlib.h n_pkey.c
nsseq.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
nsseq.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
nsseq.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
nsseq.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
nsseq.o: ../../include/openssl/ecdsa.h ../../include/openssl/evp.h
nsseq.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
nsseq.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
nsseq.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
nsseq.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
nsseq.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
nsseq.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
nsseq.o: ../../include/openssl/x509_vfy.h nsseq.c
p5_pbe.o: ../../e_os.h ../../include/openssl/asn1.h
p5_pbe.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
p5_pbe.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p5_pbe.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p5_pbe.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p5_pbe.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p5_pbe.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p5_pbe.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p5_pbe.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p5_pbe.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
p5_pbe.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
p5_pbe.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
p5_pbe.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
p5_pbe.o: ../cryptlib.h p5_pbe.c
p5_pbev2.o: ../../e_os.h ../../include/openssl/asn1.h
p5_pbev2.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
p5_pbev2.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p5_pbev2.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p5_pbev2.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p5_pbev2.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p5_pbev2.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p5_pbev2.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p5_pbev2.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p5_pbev2.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
p5_pbev2.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
p5_pbev2.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
p5_pbev2.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
p5_pbev2.o: ../cryptlib.h p5_pbev2.c
p8_pkey.o: ../../e_os.h ../../include/openssl/asn1.h
p8_pkey.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
p8_pkey.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p8_pkey.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p8_pkey.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p8_pkey.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p8_pkey.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p8_pkey.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p8_pkey.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p8_pkey.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
p8_pkey.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p8_pkey.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p8_pkey.o: ../../include/openssl/x509_vfy.h ../cryptlib.h p8_pkey.c
t_bitst.o: ../../e_os.h ../../include/openssl/asn1.h
t_bitst.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
t_bitst.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
t_bitst.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
t_bitst.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
t_bitst.o: ../../include/openssl/err.h ../../include/openssl/evp.h
t_bitst.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
t_bitst.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
t_bitst.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
t_bitst.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
t_bitst.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
t_bitst.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
t_bitst.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
t_bitst.o: ../cryptlib.h t_bitst.c
t_crl.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
t_crl.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
t_crl.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
t_crl.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
t_crl.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
t_crl.o: ../../include/openssl/err.h ../../include/openssl/evp.h
t_crl.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
t_crl.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
t_crl.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
t_crl.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
t_crl.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
t_crl.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
t_crl.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
t_crl.o: ../cryptlib.h t_crl.c
t_pkey.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
t_pkey.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
t_pkey.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
t_pkey.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
t_pkey.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
t_pkey.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
t_pkey.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
t_pkey.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
t_pkey.o: ../cryptlib.h t_pkey.c
t_req.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
t_req.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
t_req.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
t_req.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
t_req.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
t_req.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
t_req.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
t_req.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
t_req.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
t_req.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
t_req.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
t_req.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
t_req.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
t_req.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
t_req.o: ../cryptlib.h t_req.c
t_spki.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
t_spki.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
t_spki.o: ../../include/openssl/crypto.h ../../include/openssl/dsa.h
t_spki.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
t_spki.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
t_spki.o: ../../include/openssl/err.h ../../include/openssl/evp.h
t_spki.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
t_spki.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
t_spki.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
t_spki.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
t_spki.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
t_spki.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
t_spki.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
t_spki.o: ../cryptlib.h t_spki.c
t_x509.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
t_x509.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
t_x509.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
t_x509.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
t_x509.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
t_x509.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
t_x509.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
t_x509.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
t_x509.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
t_x509.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
t_x509.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
t_x509.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
t_x509.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
t_x509.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
t_x509.o: ../cryptlib.h t_x509.c
t_x509a.o: ../../e_os.h ../../include/openssl/asn1.h
t_x509a.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
t_x509a.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
t_x509a.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
t_x509a.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
t_x509a.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
t_x509a.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
t_x509a.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
t_x509a.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
t_x509a.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
t_x509a.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
t_x509a.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
t_x509a.o: ../cryptlib.h t_x509a.c
tasn_dec.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
tasn_dec.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
tasn_dec.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
tasn_dec.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
tasn_dec.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
tasn_dec.o: ../../include/openssl/opensslconf.h
tasn_dec.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tasn_dec.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
tasn_dec.o: ../../include/openssl/symhacks.h tasn_dec.c
tasn_enc.o: ../../e_os.h ../../include/openssl/asn1.h
tasn_enc.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
tasn_enc.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
tasn_enc.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
tasn_enc.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
tasn_enc.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
tasn_enc.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tasn_enc.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
tasn_enc.o: ../../include/openssl/symhacks.h ../cryptlib.h tasn_enc.c
tasn_fre.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
tasn_fre.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
tasn_fre.o: ../../include/openssl/e_os2.h ../../include/openssl/obj_mac.h
tasn_fre.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
tasn_fre.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tasn_fre.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
tasn_fre.o: ../../include/openssl/symhacks.h tasn_fre.c
tasn_new.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
tasn_new.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
tasn_new.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
tasn_new.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
tasn_new.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
tasn_new.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tasn_new.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
tasn_new.o: ../../include/openssl/symhacks.h tasn_new.c
tasn_prn.o: ../../e_os.h ../../include/openssl/asn1.h
tasn_prn.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
tasn_prn.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
tasn_prn.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
tasn_prn.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
tasn_prn.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
tasn_prn.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
tasn_prn.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
tasn_prn.o: ../../include/openssl/opensslconf.h
tasn_prn.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tasn_prn.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
tasn_prn.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
tasn_prn.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
tasn_prn.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
tasn_prn.o: ../cryptlib.h asn1_locl.h tasn_prn.c
tasn_typ.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
tasn_typ.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
tasn_typ.o: ../../include/openssl/e_os2.h ../../include/openssl/opensslconf.h
tasn_typ.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tasn_typ.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
tasn_typ.o: ../../include/openssl/symhacks.h tasn_typ.c
tasn_utl.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
tasn_utl.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
tasn_utl.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
tasn_utl.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
tasn_utl.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
tasn_utl.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
tasn_utl.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
tasn_utl.o: ../../include/openssl/symhacks.h tasn_utl.c
x_algor.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
x_algor.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
x_algor.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
x_algor.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
x_algor.o: ../../include/openssl/ecdsa.h ../../include/openssl/evp.h
x_algor.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
x_algor.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
x_algor.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_algor.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
x_algor.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
x_algor.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
x_algor.o: ../../include/openssl/x509_vfy.h x_algor.c
x_attrib.o: ../../e_os.h ../../include/openssl/asn1.h
x_attrib.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
x_attrib.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
x_attrib.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
x_attrib.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
x_attrib.o: ../../include/openssl/err.h ../../include/openssl/evp.h
x_attrib.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
x_attrib.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
x_attrib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_attrib.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
x_attrib.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
x_attrib.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
x_attrib.o: ../../include/openssl/x509_vfy.h ../cryptlib.h x_attrib.c
x_bignum.o: ../../e_os.h ../../include/openssl/asn1.h
x_bignum.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
x_bignum.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
x_bignum.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
x_bignum.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
x_bignum.o: ../../include/openssl/opensslconf.h
x_bignum.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_bignum.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
x_bignum.o: ../../include/openssl/symhacks.h ../cryptlib.h x_bignum.c
x_crl.o: ../../e_os.h ../../include/openssl/asn1.h
x_crl.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
x_crl.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
x_crl.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
x_crl.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
x_crl.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
x_crl.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
x_crl.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
x_crl.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
x_crl.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
x_crl.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
x_crl.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
x_crl.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
x_crl.o: ../../include/openssl/x509v3.h ../cryptlib.h asn1_locl.h x_crl.c
x_exten.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
x_exten.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
x_exten.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
x_exten.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
x_exten.o: ../../include/openssl/ecdsa.h ../../include/openssl/evp.h
x_exten.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
x_exten.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
x_exten.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_exten.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
x_exten.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
x_exten.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
x_exten.o: ../../include/openssl/x509_vfy.h x_exten.c
x_info.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
x_info.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
x_info.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
x_info.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
x_info.o: ../../include/openssl/err.h ../../include/openssl/evp.h
x_info.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
x_info.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
x_info.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_info.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
x_info.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
x_info.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
x_info.o: ../../include/openssl/x509_vfy.h ../cryptlib.h x_info.c
x_long.o: ../../e_os.h ../../include/openssl/asn1.h
x_long.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
x_long.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
x_long.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
x_long.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
x_long.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
x_long.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
x_long.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
x_long.o: ../cryptlib.h x_long.c
x_name.o: ../../e_os.h ../../include/openssl/asn1.h
x_name.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
x_name.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
x_name.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
x_name.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
x_name.o: ../../include/openssl/err.h ../../include/openssl/evp.h
x_name.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
x_name.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
x_name.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_name.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
x_name.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
x_name.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
x_name.o: ../../include/openssl/x509_vfy.h ../cryptlib.h asn1_locl.h x_name.c
x_nx509.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
x_nx509.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
x_nx509.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
x_nx509.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
x_nx509.o: ../../include/openssl/ecdsa.h ../../include/openssl/evp.h
x_nx509.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
x_nx509.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
x_nx509.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_nx509.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
x_nx509.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
x_nx509.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
x_nx509.o: ../../include/openssl/x509_vfy.h x_nx509.c
x_pkey.o: ../../e_os.h ../../include/openssl/asn1.h
x_pkey.o: ../../include/openssl/asn1_mac.h ../../include/openssl/bio.h
x_pkey.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
x_pkey.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
x_pkey.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
x_pkey.o: ../../include/openssl/err.h ../../include/openssl/evp.h
x_pkey.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
x_pkey.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
x_pkey.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_pkey.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
x_pkey.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
x_pkey.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
x_pkey.o: ../../include/openssl/x509_vfy.h ../cryptlib.h x_pkey.c
x_pubkey.o: ../../e_os.h ../../include/openssl/asn1.h
x_pubkey.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
x_pubkey.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
x_pubkey.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
x_pubkey.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
x_pubkey.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
x_pubkey.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
x_pubkey.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
x_pubkey.o: ../../include/openssl/opensslconf.h
x_pubkey.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_pubkey.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
x_pubkey.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
x_pubkey.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
x_pubkey.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
x_pubkey.o: ../cryptlib.h asn1_locl.h x_pubkey.c
x_req.o: ../../e_os.h ../../include/openssl/asn1.h
x_req.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
x_req.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
x_req.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
x_req.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
x_req.o: ../../include/openssl/err.h ../../include/openssl/evp.h
x_req.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
x_req.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
x_req.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_req.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
x_req.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
x_req.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
x_req.o: ../../include/openssl/x509_vfy.h ../cryptlib.h x_req.c
x_sig.o: ../../e_os.h ../../include/openssl/asn1.h
x_sig.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
x_sig.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
x_sig.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
x_sig.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
x_sig.o: ../../include/openssl/err.h ../../include/openssl/evp.h
x_sig.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
x_sig.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
x_sig.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_sig.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
x_sig.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
x_sig.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
x_sig.o: ../../include/openssl/x509_vfy.h ../cryptlib.h x_sig.c
x_spki.o: ../../e_os.h ../../include/openssl/asn1.h
x_spki.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
x_spki.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
x_spki.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
x_spki.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
x_spki.o: ../../include/openssl/err.h ../../include/openssl/evp.h
x_spki.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
x_spki.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
x_spki.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_spki.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
x_spki.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
x_spki.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
x_spki.o: ../../include/openssl/x509_vfy.h ../cryptlib.h x_spki.c
x_val.o: ../../e_os.h ../../include/openssl/asn1.h
x_val.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
x_val.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
x_val.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
x_val.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
x_val.o: ../../include/openssl/err.h ../../include/openssl/evp.h
x_val.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
x_val.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
x_val.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_val.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
x_val.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
x_val.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
x_val.o: ../../include/openssl/x509_vfy.h ../cryptlib.h x_val.c
x_x509.o: ../../e_os.h ../../include/openssl/asn1.h
x_x509.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
x_x509.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
x_x509.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
x_x509.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
x_x509.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
x_x509.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
x_x509.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
x_x509.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
x_x509.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
x_x509.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
x_x509.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
x_x509.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
x_x509.o: ../../include/openssl/x509v3.h ../cryptlib.h x_x509.c
x_x509a.o: ../../e_os.h ../../include/openssl/asn1.h
x_x509a.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
x_x509a.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
x_x509a.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
x_x509a.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
x_x509a.o: ../../include/openssl/err.h ../../include/openssl/evp.h
x_x509a.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
x_x509a.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
x_x509a.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
x_x509a.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
x_x509a.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
x_x509a.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
x_x509a.o: ../../include/openssl/x509_vfy.h ../cryptlib.h x_x509a.c
