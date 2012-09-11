#
# crypto/aes/Makefile
#

DIR=	aes
TOP=	../..
CC=	cc
CPP=	$(CC) -E
INCLUDES=
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

AES_ENC=aes_core.o aes_cbc.o

CFLAGS= $(INCLUDES) $(CFLAG)
ASFLAGS= $(INCLUDES) $(ASFLAG)
AFLAGS= $(ASFLAGS)

GENERAL=Makefile
#TEST=aestest.c
TEST=
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=aes_core.c aes_misc.c aes_ecb.c aes_cbc.c aes_cfb.c aes_ofb.c \
       aes_ctr.c aes_ige.c aes_wrap.c
LIBOBJ=aes_misc.o aes_ecb.o aes_cfb.o aes_ofb.o aes_ctr.o aes_ige.o aes_wrap.o \
       $(AES_ENC)

SRC= $(LIBSRC)

EXHEADER= aes.h
HEADER= aes_locl.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

aes-ia64.s: asm/aes-ia64.S
	$(CC) $(CFLAGS) -E asm/aes-ia64.S > $@

aes-586.s:	asm/aes-586.pl ../perlasm/x86asm.pl
	$(PERL) asm/aes-586.pl $(PERLASM_SCHEME) $(CFLAGS) $(PROCESSOR) > $@
vpaes-x86.s:	asm/vpaes-x86.pl ../perlasm/x86asm.pl
	$(PERL) asm/vpaes-x86.pl $(PERLASM_SCHEME) $(CFLAGS) $(PROCESSOR) > $@
aesni-x86.s:	asm/aesni-x86.pl ../perlasm/x86asm.pl
	$(PERL) asm/aesni-x86.pl $(PERLASM_SCHEME) $(CFLAGS) $(PROCESSOR) > $@

aes-x86_64.s: asm/aes-x86_64.pl
	$(PERL) asm/aes-x86_64.pl $(PERLASM_SCHEME) > $@
vpaes-x86_64.s:	asm/vpaes-x86_64.pl
	$(PERL) asm/vpaes-x86_64.pl $(PERLASM_SCHEME) > $@
bsaes-x86_64.s:	asm/bsaes-x86_64.pl
	$(PERL) asm/bsaes-x86_64.pl $(PERLASM_SCHEME) > $@
aesni-x86_64.s: asm/aesni-x86_64.pl
	$(PERL) asm/aesni-x86_64.pl $(PERLASM_SCHEME) > $@
aesni-sha1-x86_64.s:	asm/aesni-sha1-x86_64.pl
	$(PERL) asm/aesni-sha1-x86_64.pl $(PERLASM_SCHEME) > $@

aes-sparcv9.s: asm/aes-sparcv9.pl
	$(PERL) asm/aes-sparcv9.pl $(CFLAGS) > $@

aes-ppc.s:	asm/aes-ppc.pl
	$(PERL) asm/aes-ppc.pl $(PERLASM_SCHEME) $@

aes-parisc.s:	asm/aes-parisc.pl
	$(PERL) asm/aes-parisc.pl $(PERLASM_SCHEME) $@

aes-mips.S:	asm/aes-mips.pl
	$(PERL) asm/aes-mips.pl $(PERLASM_SCHEME) $@

# GNU make "catch all"
aes-%.S:	asm/aes-%.pl;	$(PERL) $< $(PERLASM_SCHEME) > $@
aes-armv4.o:	aes-armv4.S

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

aes_cbc.o: ../../include/openssl/aes.h ../../include/openssl/modes.h
aes_cbc.o: ../../include/openssl/opensslconf.h aes_cbc.c
aes_cfb.o: ../../include/openssl/aes.h ../../include/openssl/modes.h
aes_cfb.o: ../../include/openssl/opensslconf.h aes_cfb.c
aes_core.o: ../../include/openssl/aes.h ../../include/openssl/e_os2.h
aes_core.o: ../../include/openssl/opensslconf.h aes_core.c aes_locl.h
aes_ctr.o: ../../include/openssl/aes.h ../../include/openssl/modes.h
aes_ctr.o: ../../include/openssl/opensslconf.h aes_ctr.c
aes_ecb.o: ../../include/openssl/aes.h ../../include/openssl/e_os2.h
aes_ecb.o: ../../include/openssl/opensslconf.h aes_ecb.c aes_locl.h
aes_ige.o: ../../e_os.h ../../include/openssl/aes.h ../../include/openssl/bio.h
aes_ige.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
aes_ige.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
aes_ige.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
aes_ige.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
aes_ige.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
aes_ige.o: ../../include/openssl/symhacks.h ../cryptlib.h aes_ige.c aes_locl.h
aes_misc.o: ../../include/openssl/aes.h ../../include/openssl/crypto.h
aes_misc.o: ../../include/openssl/e_os2.h ../../include/openssl/opensslconf.h
aes_misc.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
aes_misc.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
aes_misc.o: ../../include/openssl/symhacks.h aes_locl.h aes_misc.c
aes_ofb.o: ../../include/openssl/aes.h ../../include/openssl/modes.h
aes_ofb.o: ../../include/openssl/opensslconf.h aes_ofb.c
aes_wrap.o: ../../e_os.h ../../include/openssl/aes.h
aes_wrap.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
aes_wrap.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
aes_wrap.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
aes_wrap.o: ../../include/openssl/opensslconf.h
aes_wrap.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
aes_wrap.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
aes_wrap.o: ../../include/openssl/symhacks.h ../cryptlib.h aes_wrap.c
