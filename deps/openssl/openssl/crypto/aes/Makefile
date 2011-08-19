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

AES_ASM_OBJ=aes_core.o aes_cbc.o

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
       $(AES_ASM_OBJ)

SRC= $(LIBSRC)

EXHEADER= aes.h
HEADER= aes_locl.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(ARX) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

$(LIBOBJ): $(LIBSRC)

aes-ia64.s: asm/aes-ia64.S
	$(CC) $(CFLAGS) -E asm/aes-ia64.S > $@

ax86-elf.s: asm/aes-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) aes-586.pl elf $(CFLAGS) $(PROCESSOR) > ../$@)
ax86-cof.s: asm/aes-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) aes-586.pl coff $(CFLAGS) $(PROCESSOR) > ../$@)
ax86-out.s: asm/aes-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) aes-586.pl a.out $(CFLAGS) $(PROCESSOR) > ../$@)

aes-x86_64.s: asm/aes-x86_64.pl
	$(PERL) asm/aes-x86_64.pl $@

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

aes_cbc.o: ../../include/openssl/aes.h ../../include/openssl/e_os2.h
aes_cbc.o: ../../include/openssl/opensslconf.h aes_cbc.c aes_locl.h
aes_cfb.o: ../../e_os.h ../../include/openssl/aes.h
aes_cfb.o: ../../include/openssl/e_os2.h ../../include/openssl/opensslconf.h
aes_cfb.o: aes_cfb.c aes_locl.h
aes_core.o: ../../include/openssl/aes.h ../../include/openssl/e_os2.h
aes_core.o: ../../include/openssl/fips.h ../../include/openssl/opensslconf.h
aes_core.o: aes_core.c aes_locl.h
aes_ctr.o: ../../include/openssl/aes.h ../../include/openssl/e_os2.h
aes_ctr.o: ../../include/openssl/opensslconf.h aes_ctr.c aes_locl.h
aes_ecb.o: ../../include/openssl/aes.h ../../include/openssl/e_os2.h
aes_ecb.o: ../../include/openssl/opensslconf.h aes_ecb.c aes_locl.h
aes_ige.o: ../../e_os.h ../../include/openssl/aes.h ../../include/openssl/bio.h
aes_ige.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
aes_ige.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
aes_ige.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
aes_ige.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
aes_ige.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
aes_ige.o: ../../include/openssl/symhacks.h ../cryptlib.h aes_ige.c aes_locl.h
aes_misc.o: ../../include/openssl/aes.h ../../include/openssl/e_os2.h
aes_misc.o: ../../include/openssl/opensslconf.h
aes_misc.o: ../../include/openssl/opensslv.h aes_locl.h aes_misc.c
aes_ofb.o: ../../include/openssl/aes.h ../../include/openssl/e_os2.h
aes_ofb.o: ../../include/openssl/opensslconf.h aes_locl.h aes_ofb.c
aes_wrap.o: ../../e_os.h ../../include/openssl/aes.h
aes_wrap.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
aes_wrap.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
aes_wrap.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
aes_wrap.o: ../../include/openssl/opensslconf.h
aes_wrap.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
aes_wrap.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
aes_wrap.o: ../../include/openssl/symhacks.h ../cryptlib.h aes_wrap.c
