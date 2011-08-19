#
# OpenSSL/engines/Makefile
#

DIR=	engines
TOP=	..
CC=	cc
INCLUDES= -I../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

PEX_LIBS=
EX_LIBS=

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile engines.com install.com engine_vector.mar
TEST=
APPS=

LIB=$(TOP)/libcrypto.a
LIBNAMES= 4758cca aep atalla cswift gmp chil nuron sureware ubsec capi

LIBSRC=	e_4758cca.c \
	e_aep.c \
	e_atalla.c \
	e_cswift.c \
	e_gmp.c \
	e_chil.c \
	e_nuron.c \
	e_sureware.c \
	e_ubsec.c \
	e_capi.c
LIBOBJ= e_4758cca.o \
	e_aep.o \
	e_atalla.o \
	e_cswift.o \
	e_gmp.o \
	e_chil.o \
	e_nuron.o \
	e_sureware.o \
	e_ubsec.o \
	e_capi.o

SRC= $(LIBSRC)

EXHEADER= 
HEADER=	e_4758cca_err.c e_4758cca_err.h \
	e_aep_err.c e_aep_err.h \
	e_atalla_err.c e_atalla_err.h \
	e_cswift_err.c e_cswift_err.h \
	e_gmp_err.c e_gmp_err.h \
	e_chil_err.c e_chil_err.h \
	e_nuron_err.c e_nuron_err.h \
	e_sureware_err.c e_sureware_err.h \
	e_ubsec_err.c e_ubsec_err.h \
	e_capi_err.c e_capi_err.h

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ..; $(MAKE) DIRS=$(DIR) all)

all:	lib

lib:	$(LIBOBJ)
	@if [ -n "$(SHARED_LIBS)" ]; then \
		set -e; \
		for l in $(LIBNAMES); do \
			$(MAKE) -f ../Makefile.shared -e \
				LIBNAME=$$l LIBEXTRAS=e_$$l.o \
				LIBDEPS='-L.. -lcrypto $(EX_LIBS)' \
				link_o.$(SHLIB_TARGET); \
		done; \
	else \
		$(AR) $(LIB) $(LIBOBJ); \
		$(RANLIB) $(LIB) || echo Never mind.; \
	fi; \
	touch lib

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO

links:

# XXXXX This currently only works on systems that use .so as suffix
# for shared libraries as well as for Cygwin which uses the
# dlfcn_name_converter and therefore stores the engines with .so suffix, too.
# XXXXX This was extended to HP-UX dl targets, which use .sl suffix.
install:
	@[ -n "$(INSTALLTOP)" ] # should be set by top Makefile...
	@if [ -n "$(SHARED_LIBS)" ]; then \
		set -e; \
		$(PERL) $(TOP)/util/mkdir-p.pl $(INSTALL_PREFIX)$(INSTALLTOP)/lib/engines; \
		for l in $(LIBNAMES); do \
			( echo installing $$l; \
			  if [ "$(PLATFORM)" != "Cygwin" ]; then \
				case "$(CFLAGS)" in \
				*DSO_DLFCN*)	sfx="so";;	\
				*DSO_DL*)	sfx="sl";;	\
				*)		sfx="bad";;	\
				esac; \
				cp lib$$l.$$sfx $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/lib$$l.$$sfx.new; \
			  else \
			  	sfx="so"; \
				cp cyg$$l.dll $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/lib$$l.$$sfx.new; \
			  fi; \
			  chmod 555 $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/lib$$l.$$sfx.new; \
			  mv -f $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/lib$$l.$$sfx.new $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/lib$$l.$$sfx ); \
		done; \
	fi

tags:
	ctags $(SRC)

errors:
	set -e; for l in $(LIBNAMES); do \
		$(PERL) ../util/mkerr.pl -conf e_$$l.ec \
			-nostatic -staticloader -write e_$$l.c; \
	done

tests:

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

depend:
	@if [ -z "$(THIS)" ]; then \
	    $(MAKE) -f $(TOP)/Makefile reflect THIS=$@; \
	else \
	    $(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(PROGS) $(LIBSRC); \
	fi

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

e_4758cca.o: ../include/openssl/asn1.h ../include/openssl/bio.h
e_4758cca.o: ../include/openssl/bn.h ../include/openssl/buffer.h
e_4758cca.o: ../include/openssl/crypto.h ../include/openssl/dso.h
e_4758cca.o: ../include/openssl/e_os2.h ../include/openssl/ec.h
e_4758cca.o: ../include/openssl/ecdh.h ../include/openssl/ecdsa.h
e_4758cca.o: ../include/openssl/engine.h ../include/openssl/err.h
e_4758cca.o: ../include/openssl/evp.h ../include/openssl/fips.h
e_4758cca.o: ../include/openssl/lhash.h ../include/openssl/obj_mac.h
e_4758cca.o: ../include/openssl/objects.h ../include/openssl/opensslconf.h
e_4758cca.o: ../include/openssl/opensslv.h ../include/openssl/ossl_typ.h
e_4758cca.o: ../include/openssl/pkcs7.h ../include/openssl/rand.h
e_4758cca.o: ../include/openssl/rsa.h ../include/openssl/safestack.h
e_4758cca.o: ../include/openssl/sha.h ../include/openssl/stack.h
e_4758cca.o: ../include/openssl/symhacks.h ../include/openssl/x509.h
e_4758cca.o: ../include/openssl/x509_vfy.h e_4758cca.c e_4758cca_err.c
e_4758cca.o: e_4758cca_err.h vendor_defns/hw_4758_cca.h
e_aep.o: ../include/openssl/asn1.h ../include/openssl/bio.h
e_aep.o: ../include/openssl/bn.h ../include/openssl/buffer.h
e_aep.o: ../include/openssl/crypto.h ../include/openssl/dh.h
e_aep.o: ../include/openssl/dsa.h ../include/openssl/dso.h
e_aep.o: ../include/openssl/e_os2.h ../include/openssl/ec.h
e_aep.o: ../include/openssl/ecdh.h ../include/openssl/ecdsa.h
e_aep.o: ../include/openssl/engine.h ../include/openssl/err.h
e_aep.o: ../include/openssl/evp.h ../include/openssl/fips.h
e_aep.o: ../include/openssl/lhash.h ../include/openssl/obj_mac.h
e_aep.o: ../include/openssl/objects.h ../include/openssl/opensslconf.h
e_aep.o: ../include/openssl/opensslv.h ../include/openssl/ossl_typ.h
e_aep.o: ../include/openssl/pkcs7.h ../include/openssl/rsa.h
e_aep.o: ../include/openssl/safestack.h ../include/openssl/sha.h
e_aep.o: ../include/openssl/stack.h ../include/openssl/symhacks.h
e_aep.o: ../include/openssl/x509.h ../include/openssl/x509_vfy.h e_aep.c
e_aep.o: e_aep_err.c e_aep_err.h vendor_defns/aep.h
e_atalla.o: ../include/openssl/asn1.h ../include/openssl/bio.h
e_atalla.o: ../include/openssl/bn.h ../include/openssl/buffer.h
e_atalla.o: ../include/openssl/crypto.h ../include/openssl/dh.h
e_atalla.o: ../include/openssl/dsa.h ../include/openssl/dso.h
e_atalla.o: ../include/openssl/e_os2.h ../include/openssl/ec.h
e_atalla.o: ../include/openssl/ecdh.h ../include/openssl/ecdsa.h
e_atalla.o: ../include/openssl/engine.h ../include/openssl/err.h
e_atalla.o: ../include/openssl/evp.h ../include/openssl/fips.h
e_atalla.o: ../include/openssl/lhash.h ../include/openssl/obj_mac.h
e_atalla.o: ../include/openssl/objects.h ../include/openssl/opensslconf.h
e_atalla.o: ../include/openssl/opensslv.h ../include/openssl/ossl_typ.h
e_atalla.o: ../include/openssl/pkcs7.h ../include/openssl/rsa.h
e_atalla.o: ../include/openssl/safestack.h ../include/openssl/sha.h
e_atalla.o: ../include/openssl/stack.h ../include/openssl/symhacks.h
e_atalla.o: ../include/openssl/x509.h ../include/openssl/x509_vfy.h e_atalla.c
e_atalla.o: e_atalla_err.c e_atalla_err.h vendor_defns/atalla.h
e_capi.o: ../include/openssl/asn1.h ../include/openssl/bio.h
e_capi.o: ../include/openssl/bn.h ../include/openssl/buffer.h
e_capi.o: ../include/openssl/crypto.h ../include/openssl/e_os2.h
e_capi.o: ../include/openssl/ec.h ../include/openssl/ecdh.h
e_capi.o: ../include/openssl/ecdsa.h ../include/openssl/engine.h
e_capi.o: ../include/openssl/evp.h ../include/openssl/fips.h
e_capi.o: ../include/openssl/lhash.h ../include/openssl/obj_mac.h
e_capi.o: ../include/openssl/objects.h ../include/openssl/opensslconf.h
e_capi.o: ../include/openssl/opensslv.h ../include/openssl/ossl_typ.h
e_capi.o: ../include/openssl/pkcs7.h ../include/openssl/safestack.h
e_capi.o: ../include/openssl/sha.h ../include/openssl/stack.h
e_capi.o: ../include/openssl/symhacks.h ../include/openssl/x509.h
e_capi.o: ../include/openssl/x509_vfy.h e_capi.c
e_chil.o: ../include/openssl/asn1.h ../include/openssl/bio.h
e_chil.o: ../include/openssl/bn.h ../include/openssl/buffer.h
e_chil.o: ../include/openssl/crypto.h ../include/openssl/dh.h
e_chil.o: ../include/openssl/dso.h ../include/openssl/e_os2.h
e_chil.o: ../include/openssl/ec.h ../include/openssl/ecdh.h
e_chil.o: ../include/openssl/ecdsa.h ../include/openssl/engine.h
e_chil.o: ../include/openssl/err.h ../include/openssl/evp.h
e_chil.o: ../include/openssl/fips.h ../include/openssl/lhash.h
e_chil.o: ../include/openssl/obj_mac.h ../include/openssl/objects.h
e_chil.o: ../include/openssl/opensslconf.h ../include/openssl/opensslv.h
e_chil.o: ../include/openssl/ossl_typ.h ../include/openssl/pem.h
e_chil.o: ../include/openssl/pem2.h ../include/openssl/pkcs7.h
e_chil.o: ../include/openssl/rand.h ../include/openssl/rsa.h
e_chil.o: ../include/openssl/safestack.h ../include/openssl/sha.h
e_chil.o: ../include/openssl/stack.h ../include/openssl/symhacks.h
e_chil.o: ../include/openssl/ui.h ../include/openssl/x509.h
e_chil.o: ../include/openssl/x509_vfy.h e_chil.c e_chil_err.c e_chil_err.h
e_chil.o: vendor_defns/hwcryptohook.h
e_cswift.o: ../include/openssl/asn1.h ../include/openssl/bio.h
e_cswift.o: ../include/openssl/bn.h ../include/openssl/buffer.h
e_cswift.o: ../include/openssl/crypto.h ../include/openssl/dh.h
e_cswift.o: ../include/openssl/dsa.h ../include/openssl/dso.h
e_cswift.o: ../include/openssl/e_os2.h ../include/openssl/ec.h
e_cswift.o: ../include/openssl/ecdh.h ../include/openssl/ecdsa.h
e_cswift.o: ../include/openssl/engine.h ../include/openssl/err.h
e_cswift.o: ../include/openssl/evp.h ../include/openssl/fips.h
e_cswift.o: ../include/openssl/lhash.h ../include/openssl/obj_mac.h
e_cswift.o: ../include/openssl/objects.h ../include/openssl/opensslconf.h
e_cswift.o: ../include/openssl/opensslv.h ../include/openssl/ossl_typ.h
e_cswift.o: ../include/openssl/pkcs7.h ../include/openssl/rand.h
e_cswift.o: ../include/openssl/rsa.h ../include/openssl/safestack.h
e_cswift.o: ../include/openssl/sha.h ../include/openssl/stack.h
e_cswift.o: ../include/openssl/symhacks.h ../include/openssl/x509.h
e_cswift.o: ../include/openssl/x509_vfy.h e_cswift.c e_cswift_err.c
e_cswift.o: e_cswift_err.h vendor_defns/cswift.h
e_gmp.o: ../include/openssl/asn1.h ../include/openssl/bio.h
e_gmp.o: ../include/openssl/bn.h ../include/openssl/buffer.h
e_gmp.o: ../include/openssl/crypto.h ../include/openssl/e_os2.h
e_gmp.o: ../include/openssl/ec.h ../include/openssl/ecdh.h
e_gmp.o: ../include/openssl/ecdsa.h ../include/openssl/engine.h
e_gmp.o: ../include/openssl/evp.h ../include/openssl/fips.h
e_gmp.o: ../include/openssl/lhash.h ../include/openssl/obj_mac.h
e_gmp.o: ../include/openssl/objects.h ../include/openssl/opensslconf.h
e_gmp.o: ../include/openssl/opensslv.h ../include/openssl/ossl_typ.h
e_gmp.o: ../include/openssl/pkcs7.h ../include/openssl/rsa.h
e_gmp.o: ../include/openssl/safestack.h ../include/openssl/sha.h
e_gmp.o: ../include/openssl/stack.h ../include/openssl/symhacks.h
e_gmp.o: ../include/openssl/x509.h ../include/openssl/x509_vfy.h e_gmp.c
e_nuron.o: ../include/openssl/asn1.h ../include/openssl/bio.h
e_nuron.o: ../include/openssl/bn.h ../include/openssl/buffer.h
e_nuron.o: ../include/openssl/crypto.h ../include/openssl/dh.h
e_nuron.o: ../include/openssl/dsa.h ../include/openssl/dso.h
e_nuron.o: ../include/openssl/e_os2.h ../include/openssl/ec.h
e_nuron.o: ../include/openssl/ecdh.h ../include/openssl/ecdsa.h
e_nuron.o: ../include/openssl/engine.h ../include/openssl/err.h
e_nuron.o: ../include/openssl/evp.h ../include/openssl/fips.h
e_nuron.o: ../include/openssl/lhash.h ../include/openssl/obj_mac.h
e_nuron.o: ../include/openssl/objects.h ../include/openssl/opensslconf.h
e_nuron.o: ../include/openssl/opensslv.h ../include/openssl/ossl_typ.h
e_nuron.o: ../include/openssl/pkcs7.h ../include/openssl/rsa.h
e_nuron.o: ../include/openssl/safestack.h ../include/openssl/sha.h
e_nuron.o: ../include/openssl/stack.h ../include/openssl/symhacks.h
e_nuron.o: ../include/openssl/x509.h ../include/openssl/x509_vfy.h e_nuron.c
e_nuron.o: e_nuron_err.c e_nuron_err.h
e_sureware.o: ../include/openssl/asn1.h ../include/openssl/bio.h
e_sureware.o: ../include/openssl/bn.h ../include/openssl/buffer.h
e_sureware.o: ../include/openssl/crypto.h ../include/openssl/dh.h
e_sureware.o: ../include/openssl/dsa.h ../include/openssl/dso.h
e_sureware.o: ../include/openssl/e_os2.h ../include/openssl/ec.h
e_sureware.o: ../include/openssl/ecdh.h ../include/openssl/ecdsa.h
e_sureware.o: ../include/openssl/engine.h ../include/openssl/err.h
e_sureware.o: ../include/openssl/evp.h ../include/openssl/fips.h
e_sureware.o: ../include/openssl/lhash.h ../include/openssl/obj_mac.h
e_sureware.o: ../include/openssl/objects.h ../include/openssl/opensslconf.h
e_sureware.o: ../include/openssl/opensslv.h ../include/openssl/ossl_typ.h
e_sureware.o: ../include/openssl/pem.h ../include/openssl/pem2.h
e_sureware.o: ../include/openssl/pkcs7.h ../include/openssl/rand.h
e_sureware.o: ../include/openssl/rsa.h ../include/openssl/safestack.h
e_sureware.o: ../include/openssl/sha.h ../include/openssl/stack.h
e_sureware.o: ../include/openssl/symhacks.h ../include/openssl/x509.h
e_sureware.o: ../include/openssl/x509_vfy.h e_sureware.c e_sureware_err.c
e_sureware.o: e_sureware_err.h vendor_defns/sureware.h
e_ubsec.o: ../include/openssl/asn1.h ../include/openssl/bio.h
e_ubsec.o: ../include/openssl/bn.h ../include/openssl/buffer.h
e_ubsec.o: ../include/openssl/crypto.h ../include/openssl/dh.h
e_ubsec.o: ../include/openssl/dsa.h ../include/openssl/dso.h
e_ubsec.o: ../include/openssl/e_os2.h ../include/openssl/ec.h
e_ubsec.o: ../include/openssl/ecdh.h ../include/openssl/ecdsa.h
e_ubsec.o: ../include/openssl/engine.h ../include/openssl/err.h
e_ubsec.o: ../include/openssl/evp.h ../include/openssl/fips.h
e_ubsec.o: ../include/openssl/lhash.h ../include/openssl/obj_mac.h
e_ubsec.o: ../include/openssl/objects.h ../include/openssl/opensslconf.h
e_ubsec.o: ../include/openssl/opensslv.h ../include/openssl/ossl_typ.h
e_ubsec.o: ../include/openssl/pkcs7.h ../include/openssl/rsa.h
e_ubsec.o: ../include/openssl/safestack.h ../include/openssl/sha.h
e_ubsec.o: ../include/openssl/stack.h ../include/openssl/symhacks.h
e_ubsec.o: ../include/openssl/x509.h ../include/openssl/x509_vfy.h e_ubsec.c
e_ubsec.o: e_ubsec_err.c e_ubsec_err.h vendor_defns/hw_ubsec.h
