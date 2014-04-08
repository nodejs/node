#
# OpenSSL/crypto/rand/Makefile
#

DIR=	rand
TOP=	../..
CC=	cc
INCLUDES=
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST= randtest.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=md_rand.c randfile.c rand_lib.c rand_err.c rand_egd.c \
	rand_win.c rand_unix.c rand_os2.c rand_nw.c
LIBOBJ=md_rand.o randfile.o rand_lib.o rand_err.o rand_egd.o \
	rand_win.o rand_unix.o rand_os2.o rand_nw.o

SRC= $(LIBSRC)

EXHEADER= rand.h
HEADER=	$(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

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
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

md_rand.o: ../../e_os.h ../../include/openssl/asn1.h
md_rand.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
md_rand.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
md_rand.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
md_rand.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
md_rand.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
md_rand.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rand.h
md_rand.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
md_rand.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
md_rand.o: md_rand.c rand_lcl.h
rand_egd.o: ../../include/openssl/buffer.h ../../include/openssl/e_os2.h
rand_egd.o: ../../include/openssl/opensslconf.h
rand_egd.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rand.h
rand_egd.o: rand_egd.c
rand_err.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
rand_err.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
rand_err.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
rand_err.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rand_err.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
rand_err.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rand_err.o: rand_err.c
rand_lib.o: ../../e_os.h ../../include/openssl/asn1.h
rand_lib.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
rand_lib.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
rand_lib.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
rand_lib.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
rand_lib.o: ../../include/openssl/err.h ../../include/openssl/evp.h
rand_lib.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
rand_lib.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
rand_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rand_lib.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
rand_lib.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
rand_lib.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
rand_lib.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
rand_lib.o: ../cryptlib.h rand_lib.c
rand_nw.o: ../../e_os.h ../../include/openssl/asn1.h
rand_nw.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
rand_nw.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
rand_nw.o: ../../include/openssl/err.h ../../include/openssl/evp.h
rand_nw.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
rand_nw.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
rand_nw.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rand_nw.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
rand_nw.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
rand_nw.o: ../../include/openssl/symhacks.h ../cryptlib.h rand_lcl.h rand_nw.c
rand_os2.o: ../../e_os.h ../../include/openssl/asn1.h
rand_os2.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
rand_os2.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
rand_os2.o: ../../include/openssl/err.h ../../include/openssl/evp.h
rand_os2.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
rand_os2.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
rand_os2.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rand_os2.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
rand_os2.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
rand_os2.o: ../../include/openssl/symhacks.h ../cryptlib.h rand_lcl.h
rand_os2.o: rand_os2.c
rand_unix.o: ../../e_os.h ../../include/openssl/asn1.h
rand_unix.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
rand_unix.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
rand_unix.o: ../../include/openssl/err.h ../../include/openssl/evp.h
rand_unix.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
rand_unix.o: ../../include/openssl/objects.h
rand_unix.o: ../../include/openssl/opensslconf.h
rand_unix.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rand_unix.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
rand_unix.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
rand_unix.o: ../../include/openssl/symhacks.h ../cryptlib.h rand_lcl.h
rand_unix.o: rand_unix.c
rand_win.o: ../../e_os.h ../../include/openssl/asn1.h
rand_win.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
rand_win.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
rand_win.o: ../../include/openssl/err.h ../../include/openssl/evp.h
rand_win.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
rand_win.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
rand_win.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
rand_win.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
rand_win.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
rand_win.o: ../../include/openssl/symhacks.h ../cryptlib.h rand_lcl.h
rand_win.o: rand_win.c
randfile.o: ../../e_os.h ../../include/openssl/buffer.h
randfile.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
randfile.o: ../../include/openssl/opensslconf.h
randfile.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
randfile.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
randfile.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
randfile.o: randfile.c
