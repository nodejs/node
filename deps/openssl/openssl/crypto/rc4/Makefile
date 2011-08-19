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
LIBSRC=rc4_skey.c rc4_enc.c rc4_fblk.c
LIBOBJ=$(RC4_ENC) rc4_fblk.o

SRC= $(LIBSRC)

EXHEADER= rc4.h
HEADER=	$(EXHEADER) rc4_locl.h

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(ARX) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

# ELF
rx86-elf.s: asm/rc4-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) rc4-586.pl elf $(CFLAGS) > ../$@)
# COFF
rx86-cof.s: asm/rc4-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) rc4-586.pl coff $(CFLAGS) > ../$@)
# a.out
rx86-out.s: asm/rc4-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) rc4-586.pl a.out $(CFLAGS) > ../$@)

rc4-x86_64.s: asm/rc4-x86_64.pl;	$(PERL) asm/rc4-x86_64.pl $@

rc4-ia64.s: asm/rc4-ia64.S
	@case `awk '/^#define RC4_INT/{print$$NF}' $(TOP)/include/openssl/opensslconf.h` in \
	int)	set -x; $(CC) $(CFLAGS) -DSZ=4 -E asm/rc4-ia64.S > $@ ;; \
	char)	set -x; $(CC) $(CFLAGS) -DSZ=1 -E asm/rc4-ia64.S > $@ ;; \
	*)	exit 1 ;; \
	esac

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
	rm -f *.s *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

rc4_enc.o: ../../e_os.h ../../include/openssl/bio.h
rc4_enc.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rc4_enc.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rc4_enc.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rc4_enc.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rc4_enc.o: ../../include/openssl/rc4.h ../../include/openssl/safestack.h
rc4_enc.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rc4_enc.o: ../cryptlib.h rc4_enc.c rc4_locl.h
rc4_fblk.o: ../../e_os.h ../../include/openssl/bio.h
rc4_fblk.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rc4_fblk.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rc4_fblk.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
rc4_fblk.o: ../../include/openssl/opensslconf.h
rc4_fblk.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rc4_fblk.o: ../../include/openssl/rc4.h ../../include/openssl/safestack.h
rc4_fblk.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rc4_fblk.o: ../cryptlib.h rc4_fblk.c rc4_locl.h
rc4_skey.o: ../../e_os.h ../../include/openssl/bio.h
rc4_skey.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
rc4_skey.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rc4_skey.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
rc4_skey.o: ../../include/openssl/opensslconf.h
rc4_skey.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rc4_skey.o: ../../include/openssl/rc4.h ../../include/openssl/safestack.h
rc4_skey.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rc4_skey.o: ../cryptlib.h rc4_locl.h rc4_skey.c
