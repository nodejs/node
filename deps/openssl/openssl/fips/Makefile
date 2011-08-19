#
# OpenSSL/crypto/Makefile
#

DIR=		fips
TOP=		..
CC=		cc
INCLUDE=	-I. -I$(TOP) -I../include
# INCLUDES targets sudbirs!
INCLUDES=	-I.. -I../.. -I../../include
CFLAG=		-g
MAKEDEPPROG=	makedepend
MAKEDEPEND=	$(TOP)/util/domd $(TOP) -MD $(MAKEDEPPROG)
MAKEFILE=       Makefile
RM=             rm -f
AR=		ar r
ARD=		ar d
TEST=		fips_test_suite.c
FIPS_TVDIR=	testvectors
FIPS_TVOK=	$$HOME/fips/tv.ok

FIPSCANLOC=	$(FIPSLIBDIR)fipscanister.o

RECURSIVE_MAKE=	[ -n "$(FDIRS)" ] && for i in $(FDIRS) ; do \
		    (cd $$i && echo "making $$target in $(DIR)/$$i..." && \
		    $(MAKE) -e TOP=../.. DIR=$$i INCLUDES='${INCLUDES}' $$target ) || exit 1; \
		done;

PEX_LIBS=
EX_LIBS=
 
CFLAGS= $(INCLUDE) $(CFLAG) -DHMAC_EXT=\"$${HMAC_EXT:-sha1}\"
ASFLAGS= $(INCLUDE) $(ASFLAG)
AFLAGS=$(ASFLAGS)

LIBS=

FDIRS=sha rand des aes dsa rsa dh hmac

GENERAL=Makefile README fips-lib.com install.com

LIB= $(TOP)/libcrypto.a
SHARED_LIB= $(FIPSCANLIB)$(SHLIB_EXT)
LIBSRC=fips.c 
LIBOBJ=fips.o

FIPS_OBJ_LISTS=sha/lib hmac/lib rand/lib des/lib aes/lib dsa/lib rsa/lib dh/lib

SRC= $(LIBSRC)

EXHEADER=fips.h
HEADER=$(EXHEADER) fips_utl.h fips_locl.h
EXE=fipsld

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	@(cd ..; $(MAKE) DIRS=$(DIR) all)

testapps:
	@if [ -z "$(THIS)" ]; then $(MAKE) -f $(TOP)/Makefile reflect THIS=$@; fi

all:
	@if [ -z "$(FIPSLIBDIR)" ]; then \
		$(MAKE) -e subdirs lib fips_premain_dso$(EXE_EXT); \
	else  \
		$(MAKE) -e lib fips_premain_dso$(EXE_EXT) fips_standalone_sha1$(EXE_EXT); \
	fi

# Idea behind fipscanister.o is to "seize" the sequestered code between
# known symbols for fingerprinting purposes, which would be commonly
# done with ld -r start.o ... end.o. The latter however presents a minor
# challenge on multi-ABI platforms. As just implied, we'd rather use ld,
# but the trouble is that we don't generally know how ABI-selection
# compiler flag is translated to corresponding linker flag. All compiler
# drivers seem to recognize -r flag and pass it down to linker, but some
# of them, including gcc, erroneously add -lc, as well as run-time
# components, such as crt1.o and alike. Fortunately among those vendor
# compilers which were observed to misinterpret -r flag multi-ABI ones
# are equipped with smart linkers, which don't require any ABI-selection
# flag and simply assume that all objects are of the same type as first
# one in command line. So the idea is to identify gcc and deficient
# vendor compiler drivers...

fipscanister.o: fips_start.o $(LIBOBJ) $(FIPS_OBJ_LISTS) fips_end.o
	FIPS_ASM=""; \
	list="$(BN_ASM)"; for i in $$list; do FIPS_ASM="$$FIPS_ASM ../crypto/bn/$$i" ; done; \
	list="$(AES_ASM_OBJ)"; for i in $$list; do FIPS_ASM="$$FIPS_ASM ../crypto/aes/$$i" ; done; \
	list="$(DES_ENC)"; for i in $$list; do FIPS_ASM="$$FIPS_ASM ../crypto/des/$$i" ; done; \
	list="$(SHA1_ASM_OBJ)"; for i in $$list; do FIPS_ASM="$$FIPS_ASM ../crypto/sha/$$i" ; done; \
	if [ -n "$(CPUID_OBJ)" ]; then \
		CPUID=../crypto/$(CPUID_OBJ) ; \
	else \
		CPUID="" ; \
	fi ; \
	objs="fips_start.o $(LIBOBJ) $(FIPS_EX_OBJ) $$CPUID $$FIPS_ASM"; \
	for i in $(FIPS_OBJ_LISTS); do \
		dir=`dirname $$i`; script="s|^|$$dir/|;s| | $$dir/|g"; \
		objs="$$objs `sed "$$script" $$i`"; \
	done; \
	objs="$$objs fips_end.o" ; \
	os="`(uname -s) 2>/dev/null`"; cflags="$(CFLAGS)"; \
	[ "$$os" = "AIX" ] && cflags="$$cflags -Wl,-bnoobjreorder"; \
	if [ -n "${FIPS_SITE_LD}" ]; then \
		set -x; ${FIPS_SITE_LD} -r -o $@ $$objs; \
	elif $(CC) -dumpversion >/dev/null 2>&1; then \
		set -x; $(CC) $$cflags -r -nostdlib -o $@ $$objs ; \
	else case "$$os" in \
		HP-UX|OSF1|SunOS) set -x; /usr/ccs/bin/ld -r -o $@ $$objs ;; \
		*) set -x; $(CC) $$cflags -r -o $@ $$objs ;; \
	esac fi
	./fips_standalone_sha1$(EXE_EXT) fipscanister.o > fipscanister.o.sha1

# If another exception is immediately required, assign approprite
# site-specific ld command to FIPS_SITE_LD environment variable.

fips_start.o: fips_canister.c
	$(CC) $(CFLAGS) -DFIPS_START -c -o $@ fips_canister.c
fips_end.o: fips_canister.c
	$(CC) $(CFLAGS) -DFIPS_END -c -o $@ fips_canister.c
fips_premain_dso$(EXE_EXT): fips_premain.c
	$(CC) $(CFLAGS) -DFINGERPRINT_PREMAIN_DSO_LOAD -o $@ fips_premain.c \
		$(FIPSLIBDIR)fipscanister.o ../libcrypto.a $(EX_LIBS)
# this is executed only when linking with external fipscanister.o
fips_standalone_sha1$(EXE_EXT):	sha/fips_standalone_sha1.c
	if [ -z "$(HOSTCC)" ] ; then \
		$(CC) $(CFLAGS) -DFIPSCANISTER_O -o $@ sha/fips_standalone_sha1.c $(FIPSLIBDIR)fipscanister.o $(EX_LIBS) ; \
	else \
		$(HOSTCC) $(HOSTCFLAGS) -o $ $@ -I../include -I../crypto sha/fips_standalone_sha1.c ../crypto/sha/sha1dgst.c ; \
	fi

subdirs:
	@target=all; $(RECURSIVE_MAKE)

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO
	@target=files; $(RECURSIVE_MAKE)

links:
	@$(PERL) $(TOP)/util/mklink.pl ../include/openssl $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl ../test $(TEST)
	@target=links; $(RECURSIVE_MAKE)

# lib: and $(LIB): are splitted to avoid end-less loop
lib:	$(LIB)
	if [ "$(FIPSCANISTERINTERNAL)" = "n" -a -n "$(FIPSCANLOC)" ]; then $(AR) ../$(FIPSCANLIB).a $(FIPSCANLOC); fi
	@touch lib

$(LIB):	$(FIPSLIBDIR)fipscanister.o
	$(AR) $(LIB) $(FIPSLIBDIR)fipscanister.o
	$(RANLIB) $(LIB) || echo Never mind.

$(FIPSCANLIB):	$(FIPSCANLOC)
	$(AR) ../$(FIPSCANLIB).a $(FIPSCANLOC)
	if [ "$(FIPSCANLIB)" = "libfips" ]; then \
		$(AR) $(LIB) $(FIPSCANLOC) ; \
		$(RANLIB) $(LIB) || echo Never Mind. ; \
	fi
	$(RANLIB) ../$(FIPSCANLIB).a || echo Never mind.
	@touch lib

shared:	lib subdirs fips_premain_dso$(EXE_EXT)

libs:
	@target=lib; $(RECURSIVE_MAKE)

fips_test: top
	@target=fips_test; $(RECURSIVE_MAKE)

fips_test_diff:
	@if diff -b -B -I '^\#' -cr -X fips-nodiff.txt $(FIPS_TVDIR) $(FIPS_TVOK) ; then \
		echo "FIPS diff OK" ; \
	else \
		echo "***FIPS DIFF ERROR***" ; exit 1 ; \
	fi


install:
	@[ -n "$(INSTALLTOP)" ] # should be set by top Makefile...
	@headerlist="$(EXHEADER)"; for i in $$headerlist ;\
	do \
	(cp $$i $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i; \
	chmod 644 $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i ); \
	done;
	@target=install; $(RECURSIVE_MAKE)
	for i in $(EXE) ; \
	do \
		echo "installing $$i"; \
		cp $$i $(INSTALL_PREFIX)$(INSTALLTOP)/bin/$$i.new; \
		chmod 755 $(INSTALL_PREFIX)$(INSTALLTOP)/bin/$$i.new; \
		mv -f $(INSTALL_PREFIX)$(INSTALLTOP)/bin/$$i.new $(INSTALL_PREFIX)$(INSTALLTOP)/bin/$$i; \
	done
	cp -p -f $(FIPSLIBDIR)fipscanister.o $(FIPSLIBDIR)fipscanister.o.sha1 \
		$(FIPSLIBDIR)fips_premain.c $(FIPSLIBDIR)fips_premain.c.sha1 \
		$(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/; \
	chmod 0444 $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/fips*

lint:
	@target=lint; $(RECURSIVE_MAKE)

depend:
	@[ -z "$(THIS)" ] || $(MAKEDEPEND) -- $(CFLAG) $(INCLUDE) $(DEPFLAG) -- $(PROGS) $(LIBSRC)
	@[ -z "$(THIS)" ] || (set -e; target=depend; $(RECURSIVE_MAKE) )
	@if [ -z "$(THIS)" ]; then $(MAKE) -f $(TOP)/Makefile reflect THIS=$@; fi

clean:
	rm -f fipscanister.o.sha1 fips_premain_dso$(EXE_EXT) fips_standalone_sha1$(EXE_EXT) \
		*.s *.o */*.o *.obj lib tags core .pure .nfs* *.old *.bak fluff
	@target=clean; $(RECURSIVE_MAKE)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)
	@target=dclean; $(RECURSIVE_MAKE)

# DO NOT DELETE THIS LINE -- make depend depends on it.

fips.o: ../include/openssl/asn1.h ../include/openssl/bio.h
fips.o: ../include/openssl/crypto.h ../include/openssl/des.h
fips.o: ../include/openssl/des_old.h ../include/openssl/e_os2.h
fips.o: ../include/openssl/err.h ../include/openssl/evp.h
fips.o: ../include/openssl/fips.h ../include/openssl/fips_rand.h
fips.o: ../include/openssl/hmac.h ../include/openssl/lhash.h
fips.o: ../include/openssl/obj_mac.h ../include/openssl/objects.h
fips.o: ../include/openssl/opensslconf.h ../include/openssl/opensslv.h
fips.o: ../include/openssl/ossl_typ.h ../include/openssl/rand.h
fips.o: ../include/openssl/rsa.h ../include/openssl/safestack.h
fips.o: ../include/openssl/stack.h ../include/openssl/symhacks.h
fips.o: ../include/openssl/ui.h ../include/openssl/ui_compat.h fips.c
fips.o: fips_locl.h
