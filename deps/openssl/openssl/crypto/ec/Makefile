#
# crypto/ec/Makefile
#

DIR=	ec
TOP=	../..
CC=	cc
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=ectest.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=	ec_lib.c ecp_smpl.c ecp_mont.c ecp_nist.c ec_cvt.c ec_mult.c\
	ec_err.c ec_curve.c ec_check.c ec_print.c ec_asn1.c ec_key.c\
	ec2_smpl.c ec2_mult.c ec_ameth.c ec_pmeth.c eck_prn.c

LIBOBJ=	ec_lib.o ecp_smpl.o ecp_mont.o ecp_nist.o ec_cvt.o ec_mult.o\
	ec_err.o ec_curve.o ec_check.o ec_print.o ec_asn1.o ec_key.o\
	ec2_smpl.o ec2_mult.o ec_ameth.o ec_pmeth.o eck_prn.o

SRC= $(LIBSRC)

EXHEADER= ec.h
HEADER=	ec_lcl.h $(EXHEADER)

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
	rm -f *.o */*.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

ec2_mult.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ec2_mult.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
ec2_mult.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ec2_mult.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
ec2_mult.o: ../../include/openssl/obj_mac.h ../../include/openssl/opensslconf.h
ec2_mult.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec2_mult.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ec2_mult.o: ../../include/openssl/symhacks.h ec2_mult.c ec_lcl.h
ec2_smpl.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ec2_smpl.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
ec2_smpl.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ec2_smpl.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
ec2_smpl.o: ../../include/openssl/obj_mac.h ../../include/openssl/opensslconf.h
ec2_smpl.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec2_smpl.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ec2_smpl.o: ../../include/openssl/symhacks.h ec2_smpl.c ec_lcl.h
ec_ameth.o: ../../e_os.h ../../include/openssl/asn1.h
ec_ameth.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
ec_ameth.o: ../../include/openssl/buffer.h ../../include/openssl/cms.h
ec_ameth.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
ec_ameth.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
ec_ameth.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
ec_ameth.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
ec_ameth.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
ec_ameth.o: ../../include/openssl/opensslconf.h
ec_ameth.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec_ameth.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
ec_ameth.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
ec_ameth.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
ec_ameth.o: ../../include/openssl/x509_vfy.h ../asn1/asn1_locl.h ../cryptlib.h
ec_ameth.o: ec_ameth.c
ec_asn1.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
ec_asn1.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
ec_asn1.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
ec_asn1.o: ../../include/openssl/ec.h ../../include/openssl/err.h
ec_asn1.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
ec_asn1.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
ec_asn1.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec_asn1.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ec_asn1.o: ../../include/openssl/symhacks.h ec_asn1.c ec_lcl.h
ec_check.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ec_check.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
ec_check.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ec_check.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
ec_check.o: ../../include/openssl/obj_mac.h ../../include/openssl/opensslconf.h
ec_check.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec_check.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ec_check.o: ../../include/openssl/symhacks.h ec_check.c ec_lcl.h
ec_curve.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ec_curve.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
ec_curve.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ec_curve.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
ec_curve.o: ../../include/openssl/obj_mac.h ../../include/openssl/opensslconf.h
ec_curve.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec_curve.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ec_curve.o: ../../include/openssl/symhacks.h ec_curve.c ec_lcl.h
ec_cvt.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ec_cvt.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
ec_cvt.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ec_cvt.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
ec_cvt.o: ../../include/openssl/obj_mac.h ../../include/openssl/opensslconf.h
ec_cvt.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec_cvt.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ec_cvt.o: ../../include/openssl/symhacks.h ec_cvt.c ec_lcl.h
ec_err.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ec_err.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
ec_err.o: ../../include/openssl/ec.h ../../include/openssl/err.h
ec_err.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
ec_err.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec_err.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ec_err.o: ../../include/openssl/symhacks.h ec_err.c
ec_key.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ec_key.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
ec_key.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ec_key.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
ec_key.o: ../../include/openssl/obj_mac.h ../../include/openssl/opensslconf.h
ec_key.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec_key.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ec_key.o: ../../include/openssl/symhacks.h ec_key.c ec_lcl.h
ec_lib.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ec_lib.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
ec_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ec_lib.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
ec_lib.o: ../../include/openssl/obj_mac.h ../../include/openssl/opensslconf.h
ec_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec_lib.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ec_lib.o: ../../include/openssl/symhacks.h ec_lcl.h ec_lib.c
ec_mult.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ec_mult.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
ec_mult.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ec_mult.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
ec_mult.o: ../../include/openssl/obj_mac.h ../../include/openssl/opensslconf.h
ec_mult.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec_mult.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ec_mult.o: ../../include/openssl/symhacks.h ec_lcl.h ec_mult.c
ec_pmeth.o: ../../e_os.h ../../include/openssl/asn1.h
ec_pmeth.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
ec_pmeth.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
ec_pmeth.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ec_pmeth.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
ec_pmeth.o: ../../include/openssl/err.h ../../include/openssl/evp.h
ec_pmeth.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
ec_pmeth.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
ec_pmeth.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec_pmeth.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
ec_pmeth.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
ec_pmeth.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
ec_pmeth.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../evp/evp_locl.h
ec_pmeth.o: ec_pmeth.c
ec_print.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ec_print.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
ec_print.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ec_print.o: ../../include/openssl/obj_mac.h ../../include/openssl/opensslconf.h
ec_print.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ec_print.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ec_print.o: ../../include/openssl/symhacks.h ec_lcl.h ec_print.c
eck_prn.o: ../../e_os.h ../../include/openssl/asn1.h
eck_prn.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
eck_prn.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
eck_prn.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
eck_prn.o: ../../include/openssl/err.h ../../include/openssl/evp.h
eck_prn.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
eck_prn.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
eck_prn.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
eck_prn.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
eck_prn.o: ../../include/openssl/symhacks.h ../cryptlib.h eck_prn.c
ecp_mont.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ecp_mont.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
ecp_mont.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ecp_mont.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
ecp_mont.o: ../../include/openssl/obj_mac.h ../../include/openssl/opensslconf.h
ecp_mont.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ecp_mont.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ecp_mont.o: ../../include/openssl/symhacks.h ec_lcl.h ecp_mont.c
ecp_nist.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ecp_nist.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
ecp_nist.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ecp_nist.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
ecp_nist.o: ../../include/openssl/obj_mac.h ../../include/openssl/opensslconf.h
ecp_nist.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ecp_nist.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ecp_nist.o: ../../include/openssl/symhacks.h ec_lcl.h ecp_nist.c
ecp_smpl.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
ecp_smpl.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
ecp_smpl.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
ecp_smpl.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
ecp_smpl.o: ../../include/openssl/obj_mac.h ../../include/openssl/opensslconf.h
ecp_smpl.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ecp_smpl.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ecp_smpl.o: ../../include/openssl/symhacks.h ec_lcl.h ecp_smpl.c
