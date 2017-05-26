#
# OpenSSL/crypto/rc4/Makefile
#

DIR=	rc4
TOP=	../..
CC=	cc
CPP=    $(CC) -E
INCLUDES=
CFLAG=-g
AR=		ar r

RC4_ENC=rc4_enc.o rc4_skey.o

CFLAGS= $(INCLUDES) $(CFLAG)
ASFLAGS= $(INCLUDES) $(ASFLAG)
AFLAGS= $(ASFLAGS)

GENERAL=Makefile
TEST=rc4test.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=rc4_skey.c rc4_enc.c rc4_utl.c
LIBOBJ=$(RC4_ENC) rc4_utl.o

SRC= $(LIBSRC)

EXHEADER= rc4.h
HEADER=	$(EXHEADER) rc4_locl.h

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

rc4-586.s:	asm/rc4-586.pl ../perlasm/x86asm.pl
	$(PERL) asm/rc4-586.pl $(PERLASM_SCHEME) $(CFLAGS) $(PROCESSOR) > $@

rc4-x86_64.s: asm/rc4-x86_64.pl
	$(PERL) asm/rc4-x86_64.pl $(PERLASM_SCHEME) > $@
rc4-md5-x86_64.s:	asm/rc4-md5-x86_64.pl
	$(PERL) asm/rc4-md5-x86_64.pl $(PERLASM_SCHEME) > $@

rc4-ia64.S: asm/rc4-ia64.pl
	$(PERL) asm/rc4-ia64.pl $(CFLAGS) > $@

rc4-parisc.s:	asm/rc4-parisc.pl
	$(PERL) asm/rc4-parisc.pl $(PERLASM_SCHEME) $@

rc4-ia64.s: rc4-ia64.S
	@case `awk '/^#define RC4_INT/{print$$NF}' $(TOP)/include/openssl/opensslconf.h` in \
	int)	set -x; $(CC) $(CFLAGS) -DSZ=4 -E rc4-ia64.S > $@ ;; \
	char)	set -x; $(CC) $(CFLAGS) -DSZ=1 -E rc4-ia64.S > $@ ;; \
	*)	exit 1 ;; \
	esac

# GNU make "catch all"
rc4-%.s:	asm/rc4-%.pl;	$(PERL) $< $(PERLASM_SCHEME) $@

files:
	$(PERL) $(TOP)/util/files.pl "RC4_ENC=$(RC4_ENC)" Makefile >> $(TOP)/MINFO

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

update: depend

depend:
	@[ -n "$(MAKEDEPEND)" ] # should be set by upper Makefile...
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(PROGS) $(LIBSRC)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.s *.S *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

rc4_enc.o: ../../e_os.h ../../include/openssl/bio.h
rc4_enc.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rc4_enc.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rc4_enc.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rc4_enc.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rc4_enc.o: ../../include/openssl/rc4.h ../../include/openssl/safestack.h
rc4_enc.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rc4_enc.o: ../cryptlib.h rc4_enc.c rc4_locl.h
rc4_skey.o: ../../e_os.h ../../include/openssl/bio.h
rc4_skey.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rc4_skey.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rc4_skey.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rc4_skey.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rc4_skey.o: ../../include/openssl/rc4.h ../../include/openssl/safestack.h
rc4_skey.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rc4_skey.o: ../cryptlib.h rc4_locl.h rc4_skey.c
rc4_utl.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
rc4_utl.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
rc4_utl.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rc4.h
rc4_utl.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
rc4_utl.o: ../../include/openssl/symhacks.h rc4_utl.c
