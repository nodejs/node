#
# OpenSSL/crypto/md5/Makefile
#

DIR=    md5
TOP=    ../..
CC=     cc
CPP=    $(CC) -E
INCLUDES=-I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=       Makefile
AR=             ar r

MD5_ASM_OBJ=

CFLAGS= $(INCLUDES) $(CFLAG)
ASFLAGS= $(INCLUDES) $(ASFLAG)
AFLAGS= $(ASFLAGS)

GENERAL=Makefile
TEST=md5test.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=md5_dgst.c md5_one.c
LIBOBJ=md5_dgst.o md5_one.o $(MD5_ASM_OBJ)

SRC= $(LIBSRC)

EXHEADER= md5.h
HEADER= md5_locl.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:    lib

lib:    $(LIBOBJ)
	$(ARX) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

# ELF
mx86-elf.s: asm/md5-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) md5-586.pl elf $(CFLAGS) > ../$@)
# COFF
mx86-cof.s: asm/md5-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) md5-586.pl coff $(CFLAGS) > ../$@)
# a.out
mx86-out.s: asm/md5-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) md5-586.pl a.out $(CFLAGS) > ../$@)

md5-x86_64.s:	asm/md5-x86_64.pl;	$(PERL) asm/md5-x86_64.pl $@

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

md5_dgst.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
md5_dgst.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
md5_dgst.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
md5_dgst.o: ../../include/openssl/md5.h ../../include/openssl/opensslconf.h
md5_dgst.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
md5_dgst.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
md5_dgst.o: ../../include/openssl/symhacks.h ../md32_common.h md5_dgst.c
md5_dgst.o: md5_locl.h
md5_one.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
md5_one.o: ../../include/openssl/md5.h ../../include/openssl/opensslconf.h
md5_one.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
md5_one.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
md5_one.o: ../../include/openssl/symhacks.h md5_one.c
