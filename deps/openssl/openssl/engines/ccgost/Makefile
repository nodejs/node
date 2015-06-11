DIR=ccgost
TOP=../..
CC=cc
INCLUDES= -I../../include
CFLAG=-g
MAKEFILE= Makefile
AR= ar r
CFLAGS= $(INCLUDES) $(CFLAG)
LIB=$(TOP)/libcrypto.a

LIBSRC= gost2001.c gost2001_keyx.c gost89.c gost94_keyx.c gost_ameth.c gost_asn1.c gost_crypt.c gost_ctl.c gost_eng.c gosthash.c gost_keywrap.c gost_md.c gost_params.c gost_pmeth.c gost_sign.c

LIBOBJ= e_gost_err.o gost2001_keyx.o gost2001.o gost89.o gost94_keyx.o gost_ameth.o gost_asn1.o gost_crypt.o gost_ctl.o gost_eng.o gosthash.o gost_keywrap.o gost_md.o gost_params.o gost_pmeth.o gost_sign.o

SRC=$(LIBSRC)

LIBNAME=gost

top: 
	(cd $(TOP); $(MAKE) DIRS=engines EDIRS=$(DIR) sub_all)

all: lib

tags:
	ctags $(SRC)

errors:
	$(PERL) ../../util/mkerr.pl -conf gost.ec -nostatic -write $(SRC)

lib: $(LIBOBJ)
	if [ -n "$(SHARED_LIBS)" ]; then \
		$(MAKE) -f $(TOP)/Makefile.shared -e \
			LIBNAME=$(LIBNAME) \
			LIBEXTRAS='$(LIBOBJ)' \
			LIBDEPS='-L$(TOP) -lcrypto' \
			link_o.$(SHLIB_TARGET); \
	else \
		$(AR) $(LIB) $(LIBOBJ); \
	fi
	@touch lib

install:
	[ -n "$(INSTALLTOP)" ] # should be set by top Makefile...
	if [ -n "$(SHARED_LIBS)" ]; then \
		set -e; \
		echo installing $(LIBNAME); \
		pfx=lib; \
		if expr "$(PLATFORM)" : "Cygwin" >/dev/null; then \
			sfx=".so"; \
			cp cyg$(LIBNAME).dll $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/$${pfx}$(LIBNAME)$$sfx.new; \
		else \
			case "$(CFLAGS)" in \
			*DSO_BEOS*) sfx=".so";; \
			*DSO_DLFCN*) sfx=`expr "$(SHLIB_EXT)" : '.*\(\.[a-z][a-z]*\)' \| ".so"`;; \
			*DSO_DL*) sfx=".sl";; \
			*DSO_WIN32*) sfx="eay32.dll"; pfx=;; \
			*) sfx=".bad";; \
			esac; \
			cp $${pfx}$(LIBNAME)$$sfx $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/$${pfx}$(LIBNAME)$$sfx.new; \
		fi; \
		chmod 555 $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/$${pfx}$(LIBNAME)$$sfx.new; \
		mv -f $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/$${pfx}$(LIBNAME)$$sfx.new $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/$${pfx}$(LIBNAME)$$sfx; \
	fi

links:

tests:

update: local_depend
	@if [ -z "$(THIS)" ]; then $(MAKE) -f $(TOP)/Makefile reflect THIS=$@; fi

depend: local_depend
	@if [ -z "$(THIS)" ]; then $(MAKE) -f $(TOP)/Makefile reflect THIS=$@; fi
local_depend:
	@[ -z "$(THIS)" ] || $(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(PROGS) $(LIBSRC)

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff *.so *.sl *.dll

# DO NOT DELETE THIS LINE -- make depend depends on it.

gost2001.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
gost2001.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
gost2001.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
gost2001.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
gost2001.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
gost2001.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
gost2001.o: ../../include/openssl/err.h ../../include/openssl/evp.h
gost2001.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
gost2001.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
gost2001.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
gost2001.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
gost2001.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
gost2001.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
gost2001.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
gost2001.o: e_gost_err.h gost2001.c gost89.h gost_lcl.h gost_params.h
gost2001.o: gosthash.h
gost2001_keyx.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
gost2001_keyx.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
gost2001_keyx.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
gost2001_keyx.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
gost2001_keyx.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
gost2001_keyx.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
gost2001_keyx.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
gost2001_keyx.o: ../../include/openssl/obj_mac.h
gost2001_keyx.o: ../../include/openssl/objects.h
gost2001_keyx.o: ../../include/openssl/opensslconf.h
gost2001_keyx.o: ../../include/openssl/opensslv.h
gost2001_keyx.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
gost2001_keyx.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
gost2001_keyx.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
gost2001_keyx.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
gost2001_keyx.o: ../../include/openssl/x509_vfy.h e_gost_err.h gost2001_keyx.c
gost2001_keyx.o: gost2001_keyx.h gost89.h gost_keywrap.h gost_lcl.h gosthash.h
gost89.o: gost89.c gost89.h
gost94_keyx.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
gost94_keyx.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
gost94_keyx.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
gost94_keyx.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
gost94_keyx.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
gost94_keyx.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
gost94_keyx.o: ../../include/openssl/engine.h ../../include/openssl/evp.h
gost94_keyx.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
gost94_keyx.o: ../../include/openssl/objects.h
gost94_keyx.o: ../../include/openssl/opensslconf.h
gost94_keyx.o: ../../include/openssl/opensslv.h
gost94_keyx.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
gost94_keyx.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
gost94_keyx.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
gost94_keyx.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
gost94_keyx.o: ../../include/openssl/x509_vfy.h e_gost_err.h gost89.h
gost94_keyx.o: gost94_keyx.c gost_keywrap.h gost_lcl.h gosthash.h
gost_ameth.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
gost_ameth.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
gost_ameth.o: ../../include/openssl/buffer.h ../../include/openssl/cms.h
gost_ameth.o: ../../include/openssl/crypto.h ../../include/openssl/dsa.h
gost_ameth.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
gost_ameth.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
gost_ameth.o: ../../include/openssl/engine.h ../../include/openssl/err.h
gost_ameth.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
gost_ameth.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
gost_ameth.o: ../../include/openssl/opensslconf.h
gost_ameth.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
gost_ameth.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
gost_ameth.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
gost_ameth.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
gost_ameth.o: ../../include/openssl/x509_vfy.h e_gost_err.h gost89.h
gost_ameth.o: gost_ameth.c gost_lcl.h gost_params.h gosthash.h
gost_asn1.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
gost_asn1.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
gost_asn1.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
gost_asn1.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
gost_asn1.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
gost_asn1.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
gost_asn1.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
gost_asn1.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
gost_asn1.o: ../../include/openssl/opensslconf.h
gost_asn1.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
gost_asn1.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
gost_asn1.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
gost_asn1.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
gost_asn1.o: ../../include/openssl/x509_vfy.h gost89.h gost_asn1.c gost_lcl.h
gost_asn1.o: gosthash.h
gost_crypt.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
gost_crypt.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
gost_crypt.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
gost_crypt.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
gost_crypt.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
gost_crypt.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
gost_crypt.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
gost_crypt.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
gost_crypt.o: ../../include/openssl/opensslconf.h
gost_crypt.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
gost_crypt.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
gost_crypt.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
gost_crypt.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
gost_crypt.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
gost_crypt.o: e_gost_err.h gost89.h gost_crypt.c gost_lcl.h gosthash.h
gost_ctl.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
gost_ctl.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
gost_ctl.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
gost_ctl.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
gost_ctl.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
gost_ctl.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
gost_ctl.o: ../../include/openssl/err.h ../../include/openssl/evp.h
gost_ctl.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
gost_ctl.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
gost_ctl.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
gost_ctl.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
gost_ctl.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
gost_ctl.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
gost_ctl.o: ../../include/openssl/x509_vfy.h gost89.h gost_ctl.c gost_lcl.h
gost_ctl.o: gosthash.h
gost_eng.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
gost_eng.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
gost_eng.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
gost_eng.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
gost_eng.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
gost_eng.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
gost_eng.o: ../../include/openssl/err.h ../../include/openssl/evp.h
gost_eng.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
gost_eng.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
gost_eng.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
gost_eng.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
gost_eng.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
gost_eng.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
gost_eng.o: ../../include/openssl/x509_vfy.h e_gost_err.h gost89.h gost_eng.c
gost_eng.o: gost_lcl.h gosthash.h
gost_keywrap.o: gost89.h gost_keywrap.c gost_keywrap.h
gost_md.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
gost_md.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
gost_md.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
gost_md.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
gost_md.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
gost_md.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
gost_md.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
gost_md.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
gost_md.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
gost_md.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
gost_md.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
gost_md.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
gost_md.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
gost_md.o: e_gost_err.h gost89.h gost_lcl.h gost_md.c gosthash.h
gost_params.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
gost_params.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
gost_params.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
gost_params.o: ../../include/openssl/opensslconf.h
gost_params.o: ../../include/openssl/opensslv.h
gost_params.o: ../../include/openssl/ossl_typ.h
gost_params.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
gost_params.o: ../../include/openssl/symhacks.h gost_params.c gost_params.h
gost_pmeth.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
gost_pmeth.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
gost_pmeth.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
gost_pmeth.o: ../../include/openssl/crypto.h ../../include/openssl/dsa.h
gost_pmeth.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
gost_pmeth.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
gost_pmeth.o: ../../include/openssl/engine.h ../../include/openssl/evp.h
gost_pmeth.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
gost_pmeth.o: ../../include/openssl/objects.h
gost_pmeth.o: ../../include/openssl/opensslconf.h
gost_pmeth.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
gost_pmeth.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
gost_pmeth.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
gost_pmeth.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
gost_pmeth.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
gost_pmeth.o: e_gost_err.h gost89.h gost_lcl.h gost_params.h gost_pmeth.c
gost_pmeth.o: gosthash.h
gost_sign.o: ../../include/openssl/asn1.h ../../include/openssl/asn1t.h
gost_sign.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
gost_sign.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
gost_sign.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
gost_sign.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
gost_sign.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
gost_sign.o: ../../include/openssl/err.h ../../include/openssl/evp.h
gost_sign.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
gost_sign.o: ../../include/openssl/objects.h
gost_sign.o: ../../include/openssl/opensslconf.h
gost_sign.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
gost_sign.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
gost_sign.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
gost_sign.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
gost_sign.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
gost_sign.o: e_gost_err.h gost89.h gost_lcl.h gost_params.h gost_sign.c
gost_sign.o: gosthash.h
gosthash.o: gost89.h gosthash.c gosthash.h
