#
# OpenSSL/crypto/blowfish/Makefile
#

DIR=	bf
TOP=	../..
CC=	cc
CPP=	$(CC) -E
INCLUDES=
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

BF_ENC=		bf_enc.o

CFLAGS= $(INCLUDES) $(CFLAG)
ASFLAGS= $(INCLUDES) $(ASFLAG)
AFLAGS= $(ASFLAGS)

GENERAL=Makefile
TEST=bftest.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=bf_skey.c bf_ecb.c bf_enc.c bf_cfb64.c bf_ofb64.c 
LIBOBJ=bf_skey.o bf_ecb.o $(BF_ENC) bf_cfb64.o bf_ofb64.o

SRC= $(LIBSRC)

EXHEADER= blowfish.h
HEADER=	bf_pi.h bf_locl.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

bf-586.s:	asm/bf-586.pl ../perlasm/x86asm.pl ../perlasm/cbc.pl
	$(PERL) asm/bf-586.pl $(PERLASM_SCHEME) $(CFLAGS) $(PROCESSOR) > $@

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO

links:
	@$(PERL) $(TOP)/util/mklink.pl ../../include/openssl $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl ../../test $(TEST)
	@$(PERL) $(TOP)/util/mklink.pl ../../apps $(APPS)

# We need to use force because 'install' matches 'INSTALL' on case
# insensitive systems
FRC.install:
install: FRC.install
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

bf_cfb64.o: ../../include/openssl/blowfish.h ../../include/openssl/e_os2.h
bf_cfb64.o: ../../include/openssl/opensslconf.h bf_cfb64.c bf_locl.h
bf_ecb.o: ../../include/openssl/blowfish.h ../../include/openssl/e_os2.h
bf_ecb.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
bf_ecb.o: bf_ecb.c bf_locl.h
bf_enc.o: ../../include/openssl/blowfish.h ../../include/openssl/e_os2.h
bf_enc.o: ../../include/openssl/opensslconf.h bf_enc.c bf_locl.h
bf_ofb64.o: ../../include/openssl/blowfish.h ../../include/openssl/e_os2.h
bf_ofb64.o: ../../include/openssl/opensslconf.h bf_locl.h bf_ofb64.c
bf_skey.o: ../../include/openssl/blowfish.h ../../include/openssl/crypto.h
bf_skey.o: ../../include/openssl/e_os2.h ../../include/openssl/opensslconf.h
bf_skey.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bf_skey.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bf_skey.o: ../../include/openssl/symhacks.h bf_locl.h bf_pi.h bf_skey.c
