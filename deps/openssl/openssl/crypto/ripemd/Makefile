#
# OpenSSL/crypto/ripemd/Makefile
#

DIR=    ripemd
TOP=    ../..
CC=     cc
CPP=    $(CC) -E
INCLUDES=
CFLAG=-g
MAKEFILE=       Makefile
AR=             ar r

RIP_ASM_OBJ=

CFLAGS= $(INCLUDES) $(CFLAG)
ASFLAGS= $(INCLUDES) $(ASFLAG)
AFLAGS= $(ASFLAGS)

GENERAL=Makefile
TEST=rmdtest.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=rmd_dgst.c rmd_one.c
LIBOBJ=rmd_dgst.o rmd_one.o $(RMD160_ASM_OBJ)

SRC= $(LIBSRC)

EXHEADER= ripemd.h
HEADER= rmd_locl.h rmdconst.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:    lib

lib:    $(LIBOBJ)
	$(ARX) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

# ELF
rm86-elf.s: asm/rmd-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) rmd-586.pl elf $(CFLAGS) > ../$@)
# COFF
rm86-cof.s: asm/rmd-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) rmd-586.pl coff $(CFLAGS) > ../$@)
# a.out
rm86-out.s: asm/rmd-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) rmd-586.pl a.out $(CFLAGS) > ../$@)

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

rmd_dgst.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
rmd_dgst.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rmd_dgst.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
rmd_dgst.o: ../../include/openssl/opensslconf.h
rmd_dgst.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rmd_dgst.o: ../../include/openssl/ripemd.h ../../include/openssl/safestack.h
rmd_dgst.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rmd_dgst.o: ../md32_common.h rmd_dgst.c rmd_locl.h rmdconst.h
rmd_one.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
rmd_one.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
rmd_one.o: ../../include/openssl/ossl_typ.h ../../include/openssl/ripemd.h
rmd_one.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
rmd_one.o: ../../include/openssl/symhacks.h rmd_one.c
