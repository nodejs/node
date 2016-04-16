#
# OpenSSL/crypto/bio/Makefile
#

DIR=	bio
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
LIBSRC= bio_lib.c bio_cb.c bio_err.c \
	bss_mem.c bss_null.c bss_fd.c \
	bss_file.c bss_sock.c bss_conn.c \
	bf_null.c bf_buff.c b_print.c b_dump.c \
	b_sock.c bss_acpt.c bf_nbio.c bss_log.c bss_bio.c \
	bss_dgram.c
#	bf_lbuf.c
LIBOBJ= bio_lib.o bio_cb.o bio_err.o \
	bss_mem.o bss_null.o bss_fd.o \
	bss_file.o bss_sock.o bss_conn.o \
	bf_null.o bf_buff.o b_print.o b_dump.o \
	b_sock.o bss_acpt.o bf_nbio.o bss_log.o bss_bio.o \
	bss_dgram.o
#	bf_lbuf.o

SRC= $(LIBSRC)

EXHEADER= bio.h
HEADER=	bio_lcl.h $(EXHEADER)

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
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

b_dump.o: ../../e_os.h ../../include/openssl/bio.h
b_dump.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
b_dump.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
b_dump.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
b_dump.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
b_dump.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
b_dump.o: ../../include/openssl/symhacks.h ../cryptlib.h b_dump.c bio_lcl.h
b_print.o: ../../e_os.h ../../include/openssl/bio.h ../../include/openssl/bn.h
b_print.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
b_print.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
b_print.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
b_print.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
b_print.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
b_print.o: ../../include/openssl/symhacks.h ../cryptlib.h b_print.c
b_sock.o: ../../e_os.h ../../include/openssl/bio.h
b_sock.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
b_sock.o: ../../include/openssl/dso.h ../../include/openssl/e_os2.h
b_sock.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
b_sock.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
b_sock.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
b_sock.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
b_sock.o: ../cryptlib.h b_sock.c
bf_buff.o: ../../e_os.h ../../include/openssl/bio.h
bf_buff.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bf_buff.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bf_buff.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bf_buff.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bf_buff.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bf_buff.o: ../../include/openssl/symhacks.h ../cryptlib.h bf_buff.c
bf_nbio.o: ../../e_os.h ../../include/openssl/bio.h
bf_nbio.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bf_nbio.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bf_nbio.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bf_nbio.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bf_nbio.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
bf_nbio.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
bf_nbio.o: ../cryptlib.h bf_nbio.c
bf_null.o: ../../e_os.h ../../include/openssl/bio.h
bf_null.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bf_null.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bf_null.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bf_null.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bf_null.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bf_null.o: ../../include/openssl/symhacks.h ../cryptlib.h bf_null.c
bio_cb.o: ../../e_os.h ../../include/openssl/bio.h
bio_cb.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bio_cb.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bio_cb.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bio_cb.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bio_cb.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bio_cb.o: ../../include/openssl/symhacks.h ../cryptlib.h bio_cb.c
bio_err.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
bio_err.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bio_err.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bio_err.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bio_err.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bio_err.o: ../../include/openssl/symhacks.h bio_err.c
bio_lib.o: ../../e_os.h ../../include/openssl/bio.h
bio_lib.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bio_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bio_lib.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bio_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bio_lib.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bio_lib.o: ../../include/openssl/symhacks.h ../cryptlib.h bio_lib.c
bss_acpt.o: ../../e_os.h ../../include/openssl/bio.h
bss_acpt.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bss_acpt.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bss_acpt.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bss_acpt.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bss_acpt.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bss_acpt.o: ../../include/openssl/symhacks.h ../cryptlib.h bss_acpt.c
bss_bio.o: ../../e_os.h ../../include/openssl/bio.h
bss_bio.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
bss_bio.o: ../../include/openssl/err.h ../../include/openssl/lhash.h
bss_bio.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
bss_bio.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
bss_bio.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
bss_bio.o: bss_bio.c
bss_conn.o: ../../e_os.h ../../include/openssl/bio.h
bss_conn.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bss_conn.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bss_conn.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bss_conn.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bss_conn.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bss_conn.o: ../../include/openssl/symhacks.h ../cryptlib.h bss_conn.c
bss_dgram.o: ../../e_os.h ../../include/openssl/bio.h
bss_dgram.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bss_dgram.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bss_dgram.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bss_dgram.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bss_dgram.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bss_dgram.o: ../../include/openssl/symhacks.h ../cryptlib.h bss_dgram.c
bss_fd.o: ../../e_os.h ../../include/openssl/bio.h
bss_fd.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bss_fd.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bss_fd.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bss_fd.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bss_fd.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bss_fd.o: ../../include/openssl/symhacks.h ../cryptlib.h bio_lcl.h bss_fd.c
bss_file.o: ../../e_os.h ../../include/openssl/bio.h
bss_file.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bss_file.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bss_file.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bss_file.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bss_file.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bss_file.o: ../../include/openssl/symhacks.h ../cryptlib.h bio_lcl.h bss_file.c
bss_log.o: ../../e_os.h ../../include/openssl/bio.h
bss_log.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bss_log.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bss_log.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bss_log.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bss_log.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bss_log.o: ../../include/openssl/symhacks.h ../cryptlib.h bss_log.c
bss_mem.o: ../../e_os.h ../../include/openssl/bio.h
bss_mem.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bss_mem.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bss_mem.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bss_mem.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bss_mem.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bss_mem.o: ../../include/openssl/symhacks.h ../cryptlib.h bss_mem.c
bss_null.o: ../../e_os.h ../../include/openssl/bio.h
bss_null.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bss_null.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bss_null.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bss_null.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bss_null.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bss_null.o: ../../include/openssl/symhacks.h ../cryptlib.h bss_null.c
bss_sock.o: ../../e_os.h ../../include/openssl/bio.h
bss_sock.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bss_sock.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bss_sock.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
bss_sock.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
bss_sock.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bss_sock.o: ../../include/openssl/symhacks.h ../cryptlib.h bss_sock.c
