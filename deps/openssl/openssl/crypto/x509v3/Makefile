#
# OpenSSL/crypto/x509v3/Makefile
#

DIR=	x509v3
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
LIBSRC=	v3_bcons.c v3_bitst.c v3_conf.c v3_extku.c v3_ia5.c v3_lib.c \
v3_prn.c v3_utl.c v3err.c v3_genn.c v3_alt.c v3_skey.c v3_akey.c v3_pku.c \
v3_int.c v3_enum.c v3_sxnet.c v3_cpols.c v3_crld.c v3_purp.c v3_info.c \
v3_ocsp.c v3_akeya.c v3_pmaps.c v3_pcons.c v3_ncons.c v3_pcia.c v3_pci.c \
pcy_cache.c pcy_node.c pcy_data.c pcy_map.c pcy_tree.c pcy_lib.c \
v3_asid.c v3_addr.c
LIBOBJ= v3_bcons.o v3_bitst.o v3_conf.o v3_extku.o v3_ia5.o v3_lib.o \
v3_prn.o v3_utl.o v3err.o v3_genn.o v3_alt.o v3_skey.o v3_akey.o v3_pku.o \
v3_int.o v3_enum.o v3_sxnet.o v3_cpols.o v3_crld.o v3_purp.o v3_info.o \
v3_ocsp.o v3_akeya.o v3_pmaps.o v3_pcons.o v3_ncons.o v3_pcia.o v3_pci.o \
pcy_cache.o pcy_node.o pcy_data.o pcy_map.o pcy_tree.o pcy_lib.o \
v3_asid.o v3_addr.o

SRC= $(LIBSRC)

EXHEADER= x509v3.h
HEADER=	$(EXHEADER) pcy_int.h

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
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

pcy_cache.o: ../../e_os.h ../../include/openssl/asn1.h
pcy_cache.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
pcy_cache.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
pcy_cache.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pcy_cache.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pcy_cache.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pcy_cache.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
pcy_cache.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pcy_cache.o: ../../include/openssl/opensslconf.h
pcy_cache.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pcy_cache.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
pcy_cache.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pcy_cache.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pcy_cache.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
pcy_cache.o: ../cryptlib.h pcy_cache.c pcy_int.h
pcy_data.o: ../../e_os.h ../../include/openssl/asn1.h
pcy_data.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
pcy_data.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
pcy_data.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pcy_data.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pcy_data.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pcy_data.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
pcy_data.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pcy_data.o: ../../include/openssl/opensslconf.h
pcy_data.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pcy_data.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
pcy_data.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pcy_data.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pcy_data.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
pcy_data.o: ../cryptlib.h pcy_data.c pcy_int.h
pcy_lib.o: ../../e_os.h ../../include/openssl/asn1.h
pcy_lib.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
pcy_lib.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
pcy_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pcy_lib.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pcy_lib.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pcy_lib.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
pcy_lib.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pcy_lib.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
pcy_lib.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
pcy_lib.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
pcy_lib.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
pcy_lib.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
pcy_lib.o: ../../include/openssl/x509v3.h ../cryptlib.h pcy_int.h pcy_lib.c
pcy_map.o: ../../e_os.h ../../include/openssl/asn1.h
pcy_map.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
pcy_map.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
pcy_map.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pcy_map.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pcy_map.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pcy_map.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
pcy_map.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pcy_map.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
pcy_map.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
pcy_map.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
pcy_map.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
pcy_map.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
pcy_map.o: ../../include/openssl/x509v3.h ../cryptlib.h pcy_int.h pcy_map.c
pcy_node.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pcy_node.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
pcy_node.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
pcy_node.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
pcy_node.o: ../../include/openssl/ecdsa.h ../../include/openssl/evp.h
pcy_node.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
pcy_node.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pcy_node.o: ../../include/openssl/opensslconf.h
pcy_node.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pcy_node.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
pcy_node.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pcy_node.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pcy_node.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
pcy_node.o: pcy_int.h pcy_node.c
pcy_tree.o: ../../e_os.h ../../include/openssl/asn1.h
pcy_tree.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
pcy_tree.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
pcy_tree.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pcy_tree.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pcy_tree.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pcy_tree.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
pcy_tree.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pcy_tree.o: ../../include/openssl/opensslconf.h
pcy_tree.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pcy_tree.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
pcy_tree.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pcy_tree.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pcy_tree.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
pcy_tree.o: ../cryptlib.h pcy_int.h pcy_tree.c
v3_addr.o: ../../e_os.h ../../include/openssl/asn1.h
v3_addr.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_addr.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_addr.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_addr.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_addr.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_addr.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_addr.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_addr.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_addr.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_addr.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_addr.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_addr.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_addr.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_addr.o: ../cryptlib.h v3_addr.c
v3_akey.o: ../../e_os.h ../../include/openssl/asn1.h
v3_akey.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_akey.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_akey.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_akey.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_akey.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_akey.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_akey.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_akey.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_akey.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_akey.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_akey.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_akey.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_akey.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_akey.o: ../cryptlib.h v3_akey.c
v3_akeya.o: ../../e_os.h ../../include/openssl/asn1.h
v3_akeya.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_akeya.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_akeya.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_akeya.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_akeya.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_akeya.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_akeya.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_akeya.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_akeya.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_akeya.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_akeya.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_akeya.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_akeya.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_akeya.o: ../cryptlib.h v3_akeya.c
v3_alt.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
v3_alt.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_alt.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_alt.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_alt.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_alt.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_alt.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_alt.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_alt.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_alt.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_alt.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_alt.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_alt.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_alt.o: ../cryptlib.h v3_alt.c
v3_asid.o: ../../e_os.h ../../include/openssl/asn1.h
v3_asid.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_asid.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
v3_asid.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
v3_asid.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
v3_asid.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
v3_asid.o: ../../include/openssl/err.h ../../include/openssl/evp.h
v3_asid.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
v3_asid.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
v3_asid.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
v3_asid.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
v3_asid.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
v3_asid.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
v3_asid.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
v3_asid.o: ../../include/openssl/x509v3.h ../cryptlib.h v3_asid.c
v3_bcons.o: ../../e_os.h ../../include/openssl/asn1.h
v3_bcons.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_bcons.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_bcons.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_bcons.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_bcons.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_bcons.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_bcons.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_bcons.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_bcons.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_bcons.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_bcons.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_bcons.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_bcons.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_bcons.o: ../cryptlib.h v3_bcons.c
v3_bitst.o: ../../e_os.h ../../include/openssl/asn1.h
v3_bitst.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
v3_bitst.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
v3_bitst.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
v3_bitst.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
v3_bitst.o: ../../include/openssl/err.h ../../include/openssl/evp.h
v3_bitst.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
v3_bitst.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
v3_bitst.o: ../../include/openssl/opensslconf.h
v3_bitst.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_bitst.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_bitst.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_bitst.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_bitst.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_bitst.o: ../cryptlib.h v3_bitst.c
v3_conf.o: ../../e_os.h ../../include/openssl/asn1.h
v3_conf.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
v3_conf.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
v3_conf.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
v3_conf.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
v3_conf.o: ../../include/openssl/err.h ../../include/openssl/evp.h
v3_conf.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
v3_conf.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
v3_conf.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
v3_conf.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
v3_conf.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
v3_conf.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
v3_conf.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
v3_conf.o: ../../include/openssl/x509v3.h ../cryptlib.h v3_conf.c
v3_cpols.o: ../../e_os.h ../../include/openssl/asn1.h
v3_cpols.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_cpols.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_cpols.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_cpols.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_cpols.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_cpols.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_cpols.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_cpols.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_cpols.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_cpols.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_cpols.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_cpols.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_cpols.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_cpols.o: ../cryptlib.h pcy_int.h v3_cpols.c
v3_crld.o: ../../e_os.h ../../include/openssl/asn1.h
v3_crld.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_crld.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_crld.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_crld.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_crld.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_crld.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_crld.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_crld.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_crld.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_crld.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_crld.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_crld.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_crld.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_crld.o: ../cryptlib.h v3_crld.c
v3_enum.o: ../../e_os.h ../../include/openssl/asn1.h
v3_enum.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
v3_enum.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
v3_enum.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
v3_enum.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
v3_enum.o: ../../include/openssl/err.h ../../include/openssl/evp.h
v3_enum.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
v3_enum.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
v3_enum.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
v3_enum.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
v3_enum.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
v3_enum.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
v3_enum.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
v3_enum.o: ../../include/openssl/x509v3.h ../cryptlib.h v3_enum.c
v3_extku.o: ../../e_os.h ../../include/openssl/asn1.h
v3_extku.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_extku.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_extku.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_extku.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_extku.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_extku.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_extku.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_extku.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_extku.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_extku.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_extku.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_extku.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_extku.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_extku.o: ../cryptlib.h v3_extku.c
v3_genn.o: ../../e_os.h ../../include/openssl/asn1.h
v3_genn.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_genn.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_genn.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_genn.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_genn.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_genn.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_genn.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_genn.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_genn.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_genn.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_genn.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_genn.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_genn.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_genn.o: ../cryptlib.h v3_genn.c
v3_ia5.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
v3_ia5.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_ia5.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_ia5.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_ia5.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_ia5.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_ia5.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_ia5.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_ia5.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_ia5.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_ia5.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_ia5.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_ia5.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_ia5.o: ../cryptlib.h v3_ia5.c
v3_info.o: ../../e_os.h ../../include/openssl/asn1.h
v3_info.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_info.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_info.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_info.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_info.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_info.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_info.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_info.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_info.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_info.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_info.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_info.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_info.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_info.o: ../cryptlib.h v3_info.c
v3_int.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
v3_int.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_int.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_int.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_int.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_int.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_int.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_int.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_int.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_int.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_int.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_int.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_int.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_int.o: ../cryptlib.h v3_int.c
v3_lib.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
v3_lib.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_lib.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_lib.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_lib.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_lib.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_lib.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_lib.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_lib.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_lib.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_lib.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_lib.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_lib.o: ../cryptlib.h ext_dat.h v3_lib.c
v3_ncons.o: ../../e_os.h ../../include/openssl/asn1.h
v3_ncons.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_ncons.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_ncons.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_ncons.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_ncons.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_ncons.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_ncons.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_ncons.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_ncons.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_ncons.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_ncons.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_ncons.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_ncons.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_ncons.o: ../cryptlib.h v3_ncons.c
v3_ocsp.o: ../../e_os.h ../../include/openssl/asn1.h
v3_ocsp.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
v3_ocsp.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
v3_ocsp.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
v3_ocsp.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
v3_ocsp.o: ../../include/openssl/err.h ../../include/openssl/evp.h
v3_ocsp.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
v3_ocsp.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
v3_ocsp.o: ../../include/openssl/ocsp.h ../../include/openssl/opensslconf.h
v3_ocsp.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_ocsp.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_ocsp.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_ocsp.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_ocsp.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_ocsp.o: ../cryptlib.h v3_ocsp.c
v3_pci.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
v3_pci.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_pci.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_pci.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_pci.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_pci.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_pci.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_pci.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_pci.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_pci.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_pci.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_pci.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_pci.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_pci.o: ../cryptlib.h v3_pci.c
v3_pcia.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
v3_pcia.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
v3_pcia.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
v3_pcia.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
v3_pcia.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
v3_pcia.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_pcia.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_pcia.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_pcia.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_pcia.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_pcia.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_pcia.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_pcia.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_pcia.o: v3_pcia.c
v3_pcons.o: ../../e_os.h ../../include/openssl/asn1.h
v3_pcons.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_pcons.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_pcons.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_pcons.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_pcons.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_pcons.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_pcons.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_pcons.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_pcons.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_pcons.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_pcons.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_pcons.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_pcons.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_pcons.o: ../cryptlib.h v3_pcons.c
v3_pku.o: ../../e_os.h ../../include/openssl/asn1.h
v3_pku.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_pku.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_pku.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_pku.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_pku.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_pku.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_pku.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_pku.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_pku.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_pku.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_pku.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_pku.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_pku.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_pku.o: ../cryptlib.h v3_pku.c
v3_pmaps.o: ../../e_os.h ../../include/openssl/asn1.h
v3_pmaps.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_pmaps.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_pmaps.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_pmaps.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_pmaps.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_pmaps.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_pmaps.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_pmaps.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_pmaps.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_pmaps.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_pmaps.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_pmaps.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_pmaps.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_pmaps.o: ../cryptlib.h v3_pmaps.c
v3_prn.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
v3_prn.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_prn.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_prn.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_prn.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_prn.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_prn.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_prn.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_prn.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_prn.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_prn.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_prn.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_prn.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_prn.o: ../cryptlib.h v3_prn.c
v3_purp.o: ../../e_os.h ../../include/openssl/asn1.h
v3_purp.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
v3_purp.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
v3_purp.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
v3_purp.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
v3_purp.o: ../../include/openssl/err.h ../../include/openssl/evp.h
v3_purp.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
v3_purp.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
v3_purp.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
v3_purp.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
v3_purp.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
v3_purp.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
v3_purp.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
v3_purp.o: ../../include/openssl/x509v3.h ../cryptlib.h v3_purp.c
v3_skey.o: ../../e_os.h ../../include/openssl/asn1.h
v3_skey.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
v3_skey.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
v3_skey.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
v3_skey.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
v3_skey.o: ../../include/openssl/err.h ../../include/openssl/evp.h
v3_skey.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
v3_skey.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
v3_skey.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
v3_skey.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
v3_skey.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
v3_skey.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
v3_skey.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
v3_skey.o: ../../include/openssl/x509v3.h ../cryptlib.h v3_skey.c
v3_sxnet.o: ../../e_os.h ../../include/openssl/asn1.h
v3_sxnet.o: ../../include/openssl/asn1t.h ../../include/openssl/bio.h
v3_sxnet.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3_sxnet.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3_sxnet.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3_sxnet.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3_sxnet.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3_sxnet.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3_sxnet.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3_sxnet.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3_sxnet.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3_sxnet.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3_sxnet.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3_sxnet.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3_sxnet.o: ../cryptlib.h v3_sxnet.c
v3_utl.o: ../../e_os.h ../../include/openssl/asn1.h ../../include/openssl/bio.h
v3_utl.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
v3_utl.o: ../../include/openssl/conf.h ../../include/openssl/crypto.h
v3_utl.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
v3_utl.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
v3_utl.o: ../../include/openssl/err.h ../../include/openssl/evp.h
v3_utl.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
v3_utl.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
v3_utl.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
v3_utl.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
v3_utl.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
v3_utl.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
v3_utl.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
v3_utl.o: ../../include/openssl/x509v3.h ../cryptlib.h v3_utl.c
v3err.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
v3err.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
v3err.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
v3err.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
v3err.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
v3err.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
v3err.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
v3err.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
v3err.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
v3err.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
v3err.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
v3err.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
v3err.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
v3err.o: v3err.c
