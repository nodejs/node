#
# OpenSSL/crypto/rc5/Makefile
#

DIR=	rc5
TOP=	../..
CC=	cc
CPP=	$(CC) -E
INCLUDES=
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

RC5_ENC=		rc5_enc.o
# or use
#DES_ENC=	r586-elf.o

CFLAGS= $(INCLUDES) $(CFLAG)
ASFLAGS= $(INCLUDES) $(ASFLAG)
AFLAGS= $(ASFLAGS)

GENERAL=Makefile
TEST=rc5test.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=rc5_skey.c rc5_ecb.c rc5_enc.c rc5cfb64.c rc5ofb64.c 
LIBOBJ=rc5_skey.o rc5_ecb.o $(RC5_ENC) rc5cfb64.o rc5ofb64.o

SRC= $(LIBSRC)

EXHEADER= rc5.h
HEADER=	rc5_locl.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(ARX) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

# ELF
r586-elf.s: asm/rc5-586.pl ../perlasm/x86asm.pl ../perlasm/cbc.pl
	(cd asm; $(PERL) rc5-586.pl elf $(CFLAGS) > ../$@)
# COFF
r586-cof.s: asm/rc5-586.pl ../perlasm/x86asm.pl ../perlasm/cbc.pl
	(cd asm; $(PERL) rc5-586.pl coff $(CFLAGS) > ../$@)
# a.out
r586-out.s: asm/rc5-586.pl ../perlasm/x86asm.pl ../perlasm/cbc.pl
	(cd asm; $(PERL) rc5-586.pl a.out $(CFLAGS) > ../$@)

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

rc5_ecb.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
rc5_ecb.o: ../../include/openssl/rc5.h rc5_ecb.c rc5_locl.h
rc5_enc.o: ../../include/openssl/opensslconf.h ../../include/openssl/rc5.h
rc5_enc.o: rc5_enc.c rc5_locl.h
rc5_skey.o: ../../include/openssl/opensslconf.h ../../include/openssl/rc5.h
rc5_skey.o: rc5_locl.h rc5_skey.c
rc5cfb64.o: ../../include/openssl/opensslconf.h ../../include/openssl/rc5.h
rc5cfb64.o: rc5_locl.h rc5cfb64.c
rc5ofb64.o: ../../include/openssl/opensslconf.h ../../include/openssl/rc5.h
rc5ofb64.o: rc5_locl.h rc5ofb64.c
