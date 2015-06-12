#
# OpenSSL/crypto/bn/Makefile
#

DIR=	bn
TOP=	../..
CC=	cc
CPP=    $(CC) -E
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

BN_ASM=		bn_asm.o

CFLAGS= $(INCLUDES) $(CFLAG)
ASFLAGS= $(INCLUDES) $(ASFLAG)
AFLAGS= $(ASFLAGS)

GENERAL=Makefile
TEST=bntest.c exptest.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=	bn_add.c bn_div.c bn_exp.c bn_lib.c bn_ctx.c bn_mul.c bn_mod.c \
	bn_print.c bn_rand.c bn_shift.c bn_word.c bn_blind.c \
	bn_kron.c bn_sqrt.c bn_gcd.c bn_prime.c bn_err.c bn_sqr.c bn_asm.c \
	bn_recp.c bn_mont.c bn_mpi.c bn_exp2.c bn_gf2m.c bn_nist.c \
	bn_depr.c bn_const.c bn_x931p.c

LIBOBJ=	bn_add.o bn_div.o bn_exp.o bn_lib.o bn_ctx.o bn_mul.o bn_mod.o \
	bn_print.o bn_rand.o bn_shift.o bn_word.o bn_blind.o \
	bn_kron.o bn_sqrt.o bn_gcd.o bn_prime.o bn_err.o bn_sqr.o $(BN_ASM) \
	bn_recp.o bn_mont.o bn_mpi.o bn_exp2.o bn_gf2m.o bn_nist.o \
	bn_depr.o bn_const.o bn_x931p.o

SRC= $(LIBSRC)

EXHEADER= bn.h
HEADER=	bn_lcl.h bn_prime.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

bn_prime.h: bn_prime.pl
	$(PERL) bn_prime.pl >bn_prime.h

divtest: divtest.c ../../libcrypto.a
	cc -I../../include divtest.c -o divtest ../../libcrypto.a

bnbug: bnbug.c ../../libcrypto.a top
	cc -g -I../../include bnbug.c -o bnbug ../../libcrypto.a

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

bn-586.s:	asm/bn-586.pl ../perlasm/x86asm.pl
	$(PERL) asm/bn-586.pl $(PERLASM_SCHEME) $(CFLAGS) $(PROCESSOR) > $@
co-586.s:	asm/co-586.pl ../perlasm/x86asm.pl
	$(PERL) asm/co-586.pl $(PERLASM_SCHEME) $(CFLAGS) $(PROCESSOR) > $@
x86-mont.s:	asm/x86-mont.pl ../perlasm/x86asm.pl
	$(PERL) asm/x86-mont.pl $(PERLASM_SCHEME) $(CFLAGS) $(PROCESSOR) > $@
x86-gf2m.s:	asm/x86-gf2m.pl ../perlasm/x86asm.pl
	$(PERL) asm/x86-gf2m.pl $(PERLASM_SCHEME) $(CFLAGS) $(PROCESSOR) > $@

sparcv8.o:	asm/sparcv8.S
	$(CC) $(CFLAGS) -c asm/sparcv8.S
bn-sparcv9.o:	asm/sparcv8plus.S
	$(CC) $(CFLAGS) -c -o $@ asm/sparcv8plus.S
sparcv9a-mont.s:	asm/sparcv9a-mont.pl
	$(PERL) asm/sparcv9a-mont.pl $(CFLAGS) > $@
sparcv9-mont.s:		asm/sparcv9-mont.pl
	$(PERL) asm/sparcv9-mont.pl $(CFLAGS) > $@

bn-mips3.o:	asm/mips3.s
	@if [ "$(CC)" = "gcc" ]; then \
		ABI=`expr "$(CFLAGS)" : ".*-mabi=\([n3264]*\)"` && \
		as -$$ABI -O -o $@ asm/mips3.s; \
	else	$(CC) -c $(CFLAGS) -o $@ asm/mips3.s; fi

bn-mips.s:	asm/mips.pl
	$(PERL) asm/mips.pl $(PERLASM_SCHEME) $@
mips-mont.s:	asm/mips-mont.pl
	$(PERL)	asm/mips-mont.pl $(PERLASM_SCHEME) $@

bn-s390x.o:	asm/s390x.S
	$(CC) $(CFLAGS) -c -o $@ asm/s390x.S
s390x-gf2m.s:	asm/s390x-gf2m.pl
	$(PERL) asm/s390x-gf2m.pl $(PERLASM_SCHEME) $@

x86_64-gcc.o:	asm/x86_64-gcc.c
	$(CC) $(CFLAGS) -c -o $@ asm/x86_64-gcc.c
x86_64-mont.s:	asm/x86_64-mont.pl
	$(PERL) asm/x86_64-mont.pl $(PERLASM_SCHEME) > $@
x86_64-mont5.s:	asm/x86_64-mont5.pl
	$(PERL) asm/x86_64-mont5.pl $(PERLASM_SCHEME) > $@
x86_64-gf2m.s:	asm/x86_64-gf2m.pl
	$(PERL) asm/x86_64-gf2m.pl $(PERLASM_SCHEME) > $@
modexp512-x86_64.s:	asm/modexp512-x86_64.pl
	$(PERL) asm/modexp512-x86_64.pl $(PERLASM_SCHEME) > $@

bn-ia64.s:	asm/ia64.S
	$(CC) $(CFLAGS) -E asm/ia64.S > $@
ia64-mont.s:	asm/ia64-mont.pl
	$(PERL) asm/ia64-mont.pl $@ $(CFLAGS)

# GNU assembler fails to compile PA-RISC2 modules, insist on calling
# vendor assembler...
pa-risc2W.o: asm/pa-risc2W.s
	/usr/ccs/bin/as -o pa-risc2W.o asm/pa-risc2W.s
pa-risc2.o: asm/pa-risc2.s
	/usr/ccs/bin/as -o pa-risc2.o asm/pa-risc2.s
parisc-mont.s:	asm/parisc-mont.pl
	$(PERL) asm/parisc-mont.pl $(PERLASM_SCHEME) $@

# ppc - AIX, Linux, MacOS X...
bn-ppc.s:	asm/ppc.pl;	$(PERL) asm/ppc.pl $(PERLASM_SCHEME) $@
ppc-mont.s:	asm/ppc-mont.pl;$(PERL) asm/ppc-mont.pl $(PERLASM_SCHEME) $@
ppc64-mont.s:	asm/ppc64-mont.pl;$(PERL) asm/ppc64-mont.pl $(PERLASM_SCHEME) $@

alpha-mont.s:	asm/alpha-mont.pl
	(preproc=/tmp/$$$$.$@; trap "rm $$preproc" INT; \
	$(PERL) asm/alpha-mont.pl > $$preproc && \
	$(CC) -E $$preproc > $@ && rm $$preproc)

# GNU make "catch all"
%-mont.s:	asm/%-mont.pl;	$(PERL) $< $(PERLASM_SCHEME) $@
%-gf2m.S:	asm/%-gf2m.pl;	$(PERL) $< $(PERLASM_SCHEME) $@

armv4-gf2m.o:	armv4-gf2m.S

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

exptest:
	rm -f exptest
	gcc -I../../include -g2 -ggdb -o exptest exptest.c ../../libcrypto.a

div:
	rm -f a.out
	gcc -I.. -g div.c ../../libcrypto.a

tags:
	ctags $(SRC)

tests:

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

update: bn_prime.h depend

depend:
	@[ -n "$(MAKEDEPEND)" ] # should be set by upper Makefile...
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(PROGS) $(LIBSRC)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.s *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

bn_add.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_add.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_add.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_add.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_add.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_add.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_add.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_add.c bn_lcl.h
bn_asm.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_asm.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_asm.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_asm.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_asm.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_asm.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_asm.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_asm.c bn_lcl.h
bn_blind.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_blind.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_blind.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_blind.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_blind.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_blind.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_blind.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_blind.c bn_lcl.h
bn_const.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
bn_const.o: ../../include/openssl/opensslconf.h
bn_const.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_const.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_const.o: ../../include/openssl/symhacks.h bn.h bn_const.c
bn_ctx.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_ctx.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_ctx.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_ctx.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_ctx.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_ctx.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_ctx.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_ctx.c bn_lcl.h
bn_depr.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_depr.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_depr.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_depr.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_depr.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_depr.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
bn_depr.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
bn_depr.o: ../cryptlib.h bn_depr.c bn_lcl.h
bn_div.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_div.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_div.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_div.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_div.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_div.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_div.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_div.c bn_lcl.h
bn_err.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_err.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
bn_err.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
bn_err.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
bn_err.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
bn_err.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
bn_err.o: bn_err.c
bn_exp.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_exp.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_exp.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_exp.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_exp.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_exp.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_exp.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_exp.c bn_lcl.h
bn_exp2.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_exp2.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_exp2.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_exp2.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_exp2.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_exp2.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_exp2.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_exp2.c bn_lcl.h
bn_gcd.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_gcd.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_gcd.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_gcd.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_gcd.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_gcd.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_gcd.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_gcd.c bn_lcl.h
bn_gf2m.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_gf2m.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_gf2m.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_gf2m.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_gf2m.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_gf2m.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_gf2m.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_gf2m.c bn_lcl.h
bn_kron.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_kron.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_kron.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_kron.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_kron.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_kron.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_kron.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_kron.c bn_lcl.h
bn_lib.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_lib.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_lib.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_lib.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_lib.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_lcl.h bn_lib.c
bn_mod.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_mod.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_mod.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_mod.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_mod.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_mod.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_mod.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_lcl.h bn_mod.c
bn_mont.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_mont.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_mont.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_mont.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_mont.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_mont.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_mont.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_lcl.h bn_mont.c
bn_mpi.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_mpi.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_mpi.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_mpi.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_mpi.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_mpi.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_mpi.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_lcl.h bn_mpi.c
bn_mul.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_mul.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_mul.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_mul.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_mul.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_mul.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_mul.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_lcl.h bn_mul.c
bn_nist.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_nist.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_nist.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_nist.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_nist.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_nist.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_nist.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_lcl.h bn_nist.c
bn_prime.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_prime.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_prime.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_prime.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_prime.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_prime.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
bn_prime.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
bn_prime.o: ../cryptlib.h bn_lcl.h bn_prime.c bn_prime.h
bn_print.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_print.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_print.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_print.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_print.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_print.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_print.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_lcl.h bn_print.c
bn_rand.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_rand.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_rand.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_rand.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_rand.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_rand.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
bn_rand.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
bn_rand.o: ../cryptlib.h bn_lcl.h bn_rand.c
bn_recp.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_recp.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_recp.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_recp.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_recp.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_recp.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_recp.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_lcl.h bn_recp.c
bn_shift.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_shift.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_shift.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_shift.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_shift.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_shift.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_shift.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_lcl.h bn_shift.c
bn_sqr.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_sqr.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_sqr.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_sqr.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_sqr.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_sqr.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_sqr.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_lcl.h bn_sqr.c
bn_sqrt.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_sqrt.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_sqrt.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_sqrt.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_sqrt.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_sqrt.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_sqrt.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_lcl.h bn_sqrt.c
bn_word.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
bn_word.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bn_word.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bn_word.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bn_word.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_word.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_word.o: ../../include/openssl/symhacks.h ../cryptlib.h bn_lcl.h bn_word.c
bn_x931p.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
bn_x931p.o: ../../include/openssl/e_os2.h ../../include/openssl/opensslconf.h
bn_x931p.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bn_x931p.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bn_x931p.o: ../../include/openssl/symhacks.h bn_x931p.c
