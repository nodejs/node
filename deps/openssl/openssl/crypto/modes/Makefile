#
# OpenSSL/crypto/modes/Makefile
#

DIR=	modes
TOP=	../..
CC=	cc
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

MODES_ASM_OBJ=

CFLAGS= $(INCLUDES) $(CFLAG)
ASFLAGS= $(INCLUDES) $(ASFLAG)
AFLAGS= $(ASFLAGS)

GENERAL=Makefile
TEST=
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC= cbc128.c ctr128.c cts128.c cfb128.c ofb128.c gcm128.c \
	ccm128.c xts128.c
LIBOBJ= cbc128.o ctr128.o cts128.o cfb128.o ofb128.o gcm128.o \
	ccm128.o xts128.o $(MODES_ASM_OBJ)

SRC= $(LIBSRC)

#EXHEADER= store.h str_compat.h
EXHEADER= modes.h
HEADER=	modes_lcl.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

ghash-ia64.s:	asm/ghash-ia64.pl
	$(PERL) asm/ghash-ia64.pl $@ $(CFLAGS)
ghash-x86.s:	asm/ghash-x86.pl
	$(PERL) asm/ghash-x86.pl $(PERLASM_SCHEME) $(CFLAGS) $(PROCESSOR) > $@
ghash-x86_64.s:	asm/ghash-x86_64.pl
	$(PERL) asm/ghash-x86_64.pl $(PERLASM_SCHEME) > $@
ghash-sparcv9.s:	asm/ghash-sparcv9.pl
	$(PERL) asm/ghash-sparcv9.pl $@ $(CFLAGS)
ghash-alpha.s:	asm/ghash-alpha.pl
	(preproc=/tmp/$$$$.$@; trap "rm $$preproc" INT; \
	$(PERL) asm/ghash-alpha.pl > $$preproc && \
	$(CC) -E $$preproc > $@ && rm $$preproc)

ghash-parisc.s:	asm/ghash-parisc.pl
	$(PERL) asm/ghash-parisc.pl $(PERLASM_SCHEME) $@

# GNU make "catch all"
ghash-%.S:	asm/ghash-%.pl;	$(PERL) $< $(PERLASM_SCHEME) $@

ghash-armv4.o:	ghash-armv4.S

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO

links:
	@$(PERL) $(TOP)/util/mklink.pl ../../include/openssl $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl ../../test $(TEST)
	@$(PERL) $(TOP)/util/mklink.pl ../../apps $(APPS)

install:
	@[ -n "$(INSTALLTOP)" ] # should be set by top Makefile...
	@headerlist="$(EXHEADER)"; for i in $$headerlist; \
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
	rm -f *.s *.o */*.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

cbc128.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
cbc128.o: ../../include/openssl/modes.h ../../include/openssl/opensslconf.h
cbc128.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cbc128.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
cbc128.o: ../../include/openssl/symhacks.h cbc128.c modes_lcl.h
ccm128.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
ccm128.o: ../../include/openssl/modes.h ../../include/openssl/opensslconf.h
ccm128.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ccm128.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ccm128.o: ../../include/openssl/symhacks.h ccm128.c modes_lcl.h
cfb128.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
cfb128.o: ../../include/openssl/modes.h ../../include/openssl/opensslconf.h
cfb128.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cfb128.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
cfb128.o: ../../include/openssl/symhacks.h cfb128.c modes_lcl.h
ctr128.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
ctr128.o: ../../include/openssl/modes.h ../../include/openssl/opensslconf.h
ctr128.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ctr128.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ctr128.o: ../../include/openssl/symhacks.h ctr128.c modes_lcl.h
cts128.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
cts128.o: ../../include/openssl/modes.h ../../include/openssl/opensslconf.h
cts128.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
cts128.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
cts128.o: ../../include/openssl/symhacks.h cts128.c modes_lcl.h
gcm128.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
gcm128.o: ../../include/openssl/modes.h ../../include/openssl/opensslconf.h
gcm128.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
gcm128.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
gcm128.o: ../../include/openssl/symhacks.h gcm128.c modes_lcl.h
ofb128.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
ofb128.o: ../../include/openssl/modes.h ../../include/openssl/opensslconf.h
ofb128.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
ofb128.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
ofb128.o: ../../include/openssl/symhacks.h modes_lcl.h ofb128.c
xts128.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
xts128.o: ../../include/openssl/modes.h ../../include/openssl/opensslconf.h
xts128.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
xts128.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
xts128.o: ../../include/openssl/symhacks.h modes_lcl.h xts128.c
