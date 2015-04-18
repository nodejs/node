#
# crypto/camellia/Makefile
#

DIR= camellia
TOP=	../..
CC=	cc
CPP=	$(CC) -E
INCLUDES=
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CMLL_ENC= camellia.o cmll_misc.o cmll_cbc.o

CFLAGS= $(INCLUDES) $(CFLAG)
ASFLAGS= $(INCLUDES) $(ASFLAG)
AFLAGS= $(ASFLAGS)

GENERAL=Makefile
#TEST=camelliatest.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=camellia.c cmll_misc.c cmll_ecb.c cmll_cbc.c cmll_ofb.c \
	   cmll_cfb.c cmll_ctr.c cmll_utl.c

LIBOBJ= cmll_ecb.o cmll_ofb.o cmll_cfb.o cmll_ctr.o cmll_utl.o $(CMLL_ENC)

SRC= $(LIBSRC)

EXHEADER= camellia.h
HEADER= cmll_locl.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

cmll-x86.s:	asm/cmll-x86.pl ../perlasm/x86asm.pl
	$(PERL) asm/cmll-x86.pl $(PERLASM_SCHEME) $(CFLAGS) $(PROCESSOR) > $@
cmll-x86_64.s:  asm/cmll-x86_64.pl
	$(PERL) asm/cmll-x86_64.pl $(PERLASM_SCHEME) > $@

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

camellia.o: ../../include/openssl/opensslconf.h camellia.c camellia.h
camellia.o: cmll_locl.h
cmll_cbc.o: ../../include/openssl/camellia.h ../../include/openssl/modes.h
cmll_cbc.o: ../../include/openssl/opensslconf.h cmll_cbc.c
cmll_cfb.o: ../../include/openssl/camellia.h ../../include/openssl/modes.h
cmll_cfb.o: ../../include/openssl/opensslconf.h cmll_cfb.c
cmll_ctr.o: ../../include/openssl/camellia.h ../../include/openssl/modes.h
cmll_ctr.o: ../../include/openssl/opensslconf.h cmll_ctr.c
cmll_ecb.o: ../../include/openssl/camellia.h
cmll_ecb.o: ../../include/openssl/opensslconf.h cmll_ecb.c cmll_locl.h
cmll_misc.o: ../../include/openssl/camellia.h ../../include/openssl/crypto.h
cmll_misc.o: ../../include/openssl/e_os2.h ../../include/openssl/opensslconf.h
cmll_misc.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cmll_misc.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
cmll_misc.o: ../../include/openssl/symhacks.h cmll_locl.h cmll_misc.c
cmll_ofb.o: ../../include/openssl/camellia.h ../../include/openssl/modes.h
cmll_ofb.o: ../../include/openssl/opensslconf.h cmll_ofb.c
cmll_utl.o: ../../include/openssl/camellia.h ../../include/openssl/crypto.h
cmll_utl.o: ../../include/openssl/e_os2.h ../../include/openssl/opensslconf.h
cmll_utl.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cmll_utl.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
cmll_utl.o: ../../include/openssl/symhacks.h cmll_locl.h cmll_utl.c
