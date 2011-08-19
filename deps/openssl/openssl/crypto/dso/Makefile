#
# OpenSSL/crypto/dso/Makefile
#

DIR=	dso
TOP=	../..
CC=	cc
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC= dso_dl.c dso_dlfcn.c dso_err.c dso_lib.c dso_null.c \
	dso_openssl.c dso_win32.c dso_vms.c
LIBOBJ= dso_dl.o dso_dlfcn.o dso_err.o dso_lib.o dso_null.o \
	dso_openssl.o dso_win32.o dso_vms.o

SRC= $(LIBSRC)

EXHEADER= dso.h
HEADER=	$(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(ARX) $(LIB) $(LIBOBJ)
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
	rm -f *.o */*.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

dso_dl.o: ../../e_os.h ../../include/openssl/bio.h
dso_dl.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
dso_dl.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
dso_dl.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dso_dl.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
dso_dl.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
dso_dl.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
dso_dl.o: ../cryptlib.h dso_dl.c
dso_dlfcn.o: ../../e_os.h ../../include/openssl/bio.h
dso_dlfcn.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
dso_dlfcn.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
dso_dlfcn.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dso_dlfcn.o: ../../include/openssl/opensslconf.h
dso_dlfcn.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
dso_dlfcn.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
dso_dlfcn.o: ../../include/openssl/symhacks.h ../cryptlib.h dso_dlfcn.c
dso_err.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
dso_err.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
dso_err.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dso_err.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
dso_err.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
dso_err.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
dso_err.o: dso_err.c
dso_lib.o: ../../e_os.h ../../include/openssl/bio.h
dso_lib.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
dso_lib.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
dso_lib.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dso_lib.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
dso_lib.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
dso_lib.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
dso_lib.o: ../cryptlib.h dso_lib.c
dso_null.o: ../../e_os.h ../../include/openssl/bio.h
dso_null.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
dso_null.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
dso_null.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dso_null.o: ../../include/openssl/opensslconf.h
dso_null.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
dso_null.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
dso_null.o: ../../include/openssl/symhacks.h ../cryptlib.h dso_null.c
dso_openssl.o: ../../e_os.h ../../include/openssl/bio.h
dso_openssl.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
dso_openssl.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
dso_openssl.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dso_openssl.o: ../../include/openssl/opensslconf.h
dso_openssl.o: ../../include/openssl/opensslv.h
dso_openssl.o: ../../include/openssl/ossl_typ.h
dso_openssl.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
dso_openssl.o: ../../include/openssl/symhacks.h ../cryptlib.h dso_openssl.c
dso_vms.o: ../../e_os.h ../../include/openssl/bio.h
dso_vms.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
dso_vms.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
dso_vms.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dso_vms.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
dso_vms.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
dso_vms.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
dso_vms.o: ../cryptlib.h dso_vms.c
dso_win32.o: ../../e_os.h ../../include/openssl/bio.h
dso_win32.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
dso_win32.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
dso_win32.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
dso_win32.o: ../../include/openssl/opensslconf.h
dso_win32.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
dso_win32.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
dso_win32.o: ../../include/openssl/symhacks.h ../cryptlib.h dso_win32.c
