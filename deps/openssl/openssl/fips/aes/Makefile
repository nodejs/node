#
# OpenSSL/fips/aes/Makefile
#

DIR=	aes
TOP=	../..
CC=	cc
INCLUDES=
CFLAG=-g
INSTALL_PREFIX=
OPENSSLDIR=     /usr/local/ssl
INSTALLTOP=/usr/local/ssl
MAKEDEPPROG=	makedepend
MAKEDEPEND=	$(TOP)/util/domd $(TOP) -MD $(MAKEDEPPROG)
MAKEFILE=	Makefile
AR=		ar r

ASFLAGS= $(INCLUDES) $(ASFLAG)
AFLAGS= $(ASFLAGS)

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=fips_aesavs.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=fips_aes_selftest.c
LIBOBJ=fips_aes_selftest.o

SRC= $(LIBSRC)

EXHEADER=
HEADER=

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd $(TOP); $(MAKE) DIRS=fips FDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	@echo $(LIBOBJ) > lib

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO

links:
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/include/openssl $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/test $(TEST)
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/apps $(APPS)

install:
	@headerlist="$(EXHEADER)"; for i in $$headerlist; \
	do  \
	  (cp $$i $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i; \
	  chmod 644 $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i ); \
	done

tags:
	ctags $(SRC)

tests:

fips_test:
	-find ../testvectors/aes/req -name '*.req' > testlist
	-rm -rf ../testvectors/aes/rsp
	mkdir ../testvectors/aes/rsp
	if [ -s testlist ]; then $(TOP)/util/shlib_wrap.sh $(TOP)/test/fips_aesavs -d testlist; fi

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

depend:
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(PROGS) \
		$(SRC) $(TEST)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o asm/*.o *.obj lib tags core .pure .nfs* *.old *.bak fluff testlist
# DO NOT DELETE THIS LINE -- make depend depends on it.

fips_aes_selftest.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_aes_selftest.o: ../../include/openssl/crypto.h
fips_aes_selftest.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_aes_selftest.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
fips_aes_selftest.o: ../../include/openssl/lhash.h
fips_aes_selftest.o: ../../include/openssl/obj_mac.h
fips_aes_selftest.o: ../../include/openssl/objects.h
fips_aes_selftest.o: ../../include/openssl/opensslconf.h
fips_aes_selftest.o: ../../include/openssl/opensslv.h
fips_aes_selftest.o: ../../include/openssl/ossl_typ.h
fips_aes_selftest.o: ../../include/openssl/safestack.h
fips_aes_selftest.o: ../../include/openssl/stack.h
fips_aes_selftest.o: ../../include/openssl/symhacks.h fips_aes_selftest.c
fips_aesavs.o: ../../e_os.h ../../include/openssl/aes.h
fips_aesavs.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_aesavs.o: ../../include/openssl/bn.h ../../include/openssl/crypto.h
fips_aesavs.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_aesavs.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
fips_aesavs.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
fips_aesavs.o: ../../include/openssl/objects.h
fips_aesavs.o: ../../include/openssl/opensslconf.h
fips_aesavs.o: ../../include/openssl/opensslv.h
fips_aesavs.o: ../../include/openssl/ossl_typ.h
fips_aesavs.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
fips_aesavs.o: ../../include/openssl/symhacks.h ../fips_utl.h fips_aesavs.c
