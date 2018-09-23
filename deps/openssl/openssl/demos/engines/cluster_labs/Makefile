LIBNAME=	libclabs
SRC=		hw_cluster_labs.c
OBJ=		hw_cluster_labs.o
HEADER=		hw_cluster_labs.h

CC=		gcc
PIC=		-fPIC
CFLAGS=		-g -I../../../include $(PIC) -DENGINE_DYNAMIC_SUPPORT -DFLAT_INC
AR=		ar r
RANLIB=		ranlib

LIB=		$(LIBNAME).a
SHLIB=		$(LIBNAME).so

all:
		@echo 'Please choose a system to build on:'
		@echo ''
		@echo 'tru64:    Tru64 Unix, Digital Unix, Digital OSF/1'
		@echo 'solaris:  Solaris'
		@echo 'irix:     IRIX'
		@echo 'hpux32:   32-bit HP/UX'
		@echo 'hpux64:   64-bit HP/UX'
		@echo 'aix:      AIX'
		@echo 'gnu:      Generic GNU-based system (gcc and GNU ld)'
		@echo ''

FORCE.update:
update:		FORCE.update
		perl ../../../util/mkerr.pl -conf hw_cluster_labs.ec \
			-nostatic -staticloader -write hw_cluster_labs.c

gnu:		$(SHLIB).gnu
tru64:		$(SHLIB).tru64
solaris:	$(SHLIB).solaris
irix:		$(SHLIB).irix
hpux32:		$(SHLIB).hpux32
hpux64:		$(SHLIB).hpux64
aix:		$(SHLIB).aix

$(LIB):		$(OBJ)
		$(AR) $(LIB) $(OBJ)
		- $(RANLIB) $(LIB)

LINK_SO=	\
  ld -r -o $(LIBNAME).o $$ALLSYMSFLAGS $(LIB) && \
  (nm -Pg $(LIBNAME).o | grep ' [BDT] ' | cut -f1 -d' ' > $(LIBNAME).exp; \
   $$SHAREDCMD $$SHAREDFLAGS -o $(SHLIB) $(LIBNAME).o -L ../../.. -lcrypto -lc)

$(SHLIB).gnu:	$(LIB)
		ALLSYMSFLAGS='--whole-archive' \
		SHAREDFLAGS='-shared -Wl,-soname=$(SHLIB)' \
		SHAREDCMD='$(CC)'; \
		$(LINK_SO)
		touch $(SHLIB).gnu
$(SHLIB).tru64:	$(LIB)
		ALLSYMSFLAGS='-all' \
		SHAREDFLAGS='-shared' \
		SHAREDCMD='$(CC)'; \
		$(LINK_SO)
		touch $(SHLIB).tru64
$(SHLIB).solaris:	$(LIB)
		ALLSYMSFLAGS='-z allextract' \
		SHAREDFLAGS='-G -h $(SHLIB)' \
		SHAREDCMD='$(CC)'; \
		$(LINK_SO)
		touch $(SHLIB).solaris
$(SHLIB).irix:	$(LIB)
		ALLSYMSFLAGS='-all' \
		SHAREDFLAGS='-shared -Wl,-soname,$(SHLIB)' \
		SHAREDCMD='$(CC)'; \
		$(LINK_SO)
		touch $(SHLIB).irix
$(SHLIB).hpux32:	$(LIB)
		ALLSYMSFLAGS='-Fl' \
		SHAREDFLAGS='+vnocompatwarnings -b -z +s +h $(SHLIB)' \
		SHAREDCMD='/usr/ccs/bin/ld'; \
		$(LINK_SO)
		touch $(SHLIB).hpux32
$(SHLIB).hpux64:	$(LIB)
		ALLSYMSFLAGS='+forceload' \
		SHAREDFLAGS='-b -z +h $(SHLIB)' \
		SHAREDCMD='/usr/ccs/bin/ld'; \
		$(LINK_SO)
		touch $(SHLIB).hpux64
$(SHLIB).aix:	$(LIB)
		ALLSYMSFLAGS='-bnogc' \
		SHAREDFLAGS='-G -bE:$(LIBNAME).exp -bM:SRE' \
		SHAREDCMD='$(CC)'; \
		$(LINK_SO)
		touch $(SHLIB).aix

depend:
		sed -e '/^# DO NOT DELETE.*/,$$d' < Makefile > Makefile.tmp
		echo '# DO NOT DELETE THIS LINE -- make depend depends on it.' >> Makefile.tmp
		gcc -M $(CFLAGS) $(SRC) >> Makefile.tmp
		perl ../../../util/clean-depend.pl < Makefile.tmp > Makefile.new
		rm -f Makefile.tmp Makefile
		mv Makefile.new Makefile

# DO NOT DELETE THIS LINE -- make depend depends on it.

rsaref.o: ../../../include/openssl/asn1.h ../../../include/openssl/bio.h
rsaref.o: ../../../include/openssl/bn.h ../../../include/openssl/crypto.h
rsaref.o: ../../../include/openssl/dh.h ../../../include/openssl/dsa.h
rsaref.o: ../../../include/openssl/e_os2.h ../../../include/openssl/engine.h
rsaref.o: ../../../include/openssl/err.h ../../../include/openssl/lhash.h
rsaref.o: ../../../include/openssl/opensslconf.h
rsaref.o: ../../../include/openssl/opensslv.h
rsaref.o: ../../../include/openssl/ossl_typ.h ../../../include/openssl/rand.h
rsaref.o: ../../../include/openssl/rsa.h ../../../include/openssl/safestack.h
rsaref.o: ../../../include/openssl/stack.h ../../../include/openssl/symhacks.h
rsaref.o: ../../../include/openssl/ui.h rsaref.c rsaref_err.c rsaref_err.h
rsaref.o: source/des.h source/global.h source/md2.h source/md5.h source/rsa.h
rsaref.o: source/rsaref.h
