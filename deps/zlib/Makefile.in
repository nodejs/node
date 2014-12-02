# Makefile for zlib
# Copyright (C) 1995-2013 Jean-loup Gailly, Mark Adler
# For conditions of distribution and use, see copyright notice in zlib.h

# To compile and test, type:
#    ./configure; make test
# Normally configure builds both a static and a shared library.
# If you want to build just a static library, use: ./configure --static

# To use the asm code, type:
#    cp contrib/asm?86/match.S ./match.S
#    make LOC=-DASMV OBJA=match.o

# To install /usr/local/lib/libz.* and /usr/local/include/zlib.h, type:
#    make install
# To install in $HOME instead of /usr/local, use:
#    make install prefix=$HOME

CC=cc

CFLAGS=-O
#CFLAGS=-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7
#CFLAGS=-g -DDEBUG
#CFLAGS=-O3 -Wall -Wwrite-strings -Wpointer-arith -Wconversion \
#           -Wstrict-prototypes -Wmissing-prototypes

SFLAGS=-O
LDFLAGS=
TEST_LDFLAGS=-L. libz.a
LDSHARED=$(CC)
CPP=$(CC) -E

STATICLIB=libz.a
SHAREDLIB=libz.so
SHAREDLIBV=libz.so.1.2.8
SHAREDLIBM=libz.so.1
LIBS=$(STATICLIB) $(SHAREDLIBV)

AR=ar
ARFLAGS=rc
RANLIB=ranlib
LDCONFIG=ldconfig
LDSHAREDLIBC=-lc
TAR=tar
SHELL=/bin/sh
EXE=

prefix = /usr/local
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
sharedlibdir = ${libdir}
includedir = ${prefix}/include
mandir = ${prefix}/share/man
man3dir = ${mandir}/man3
pkgconfigdir = ${libdir}/pkgconfig

OBJZ = adler32.o crc32.o deflate.o infback.o inffast.o inflate.o inftrees.o trees.o zutil.o
OBJG = compress.o uncompr.o gzclose.o gzlib.o gzread.o gzwrite.o
OBJC = $(OBJZ) $(OBJG)

PIC_OBJZ = adler32.lo crc32.lo deflate.lo infback.lo inffast.lo inflate.lo inftrees.lo trees.lo zutil.lo
PIC_OBJG = compress.lo uncompr.lo gzclose.lo gzlib.lo gzread.lo gzwrite.lo
PIC_OBJC = $(PIC_OBJZ) $(PIC_OBJG)

# to use the asm code: make OBJA=match.o, PIC_OBJA=match.lo
OBJA =
PIC_OBJA =

OBJS = $(OBJC) $(OBJA)

PIC_OBJS = $(PIC_OBJC) $(PIC_OBJA)

all: static shared

static: example$(EXE) minigzip$(EXE)

shared: examplesh$(EXE) minigzipsh$(EXE)

all64: example64$(EXE) minigzip64$(EXE)

check: test

test: all teststatic testshared

teststatic: static
	@TMPST=tmpst_$$; \
	if echo hello world | ./minigzip | ./minigzip -d && ./example $$TMPST ; then \
	  echo '		*** zlib test OK ***'; \
	else \
	  echo '		*** zlib test FAILED ***'; false; \
	fi; \
	rm -f $$TMPST

testshared: shared
	@LD_LIBRARY_PATH=`pwd`:$(LD_LIBRARY_PATH) ; export LD_LIBRARY_PATH; \
	LD_LIBRARYN32_PATH=`pwd`:$(LD_LIBRARYN32_PATH) ; export LD_LIBRARYN32_PATH; \
	DYLD_LIBRARY_PATH=`pwd`:$(DYLD_LIBRARY_PATH) ; export DYLD_LIBRARY_PATH; \
	SHLIB_PATH=`pwd`:$(SHLIB_PATH) ; export SHLIB_PATH; \
	TMPSH=tmpsh_$$; \
	if echo hello world | ./minigzipsh | ./minigzipsh -d && ./examplesh $$TMPSH; then \
	  echo '		*** zlib shared test OK ***'; \
	else \
	  echo '		*** zlib shared test FAILED ***'; false; \
	fi; \
	rm -f $$TMPSH

test64: all64
	@TMP64=tmp64_$$; \
	if echo hello world | ./minigzip64 | ./minigzip64 -d && ./example64 $$TMP64; then \
	  echo '		*** zlib 64-bit test OK ***'; \
	else \
	  echo '		*** zlib 64-bit test FAILED ***'; false; \
	fi; \
	rm -f $$TMP64

infcover.o: test/infcover.c zlib.h zconf.h
	$(CC) $(CFLAGS) -I. -c -o $@ test/infcover.c

infcover: infcover.o libz.a
	$(CC) $(CFLAGS) -o $@ infcover.o libz.a

cover: infcover
	rm -f *.gcda
	./infcover
	gcov inf*.c

libz.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

match.o: match.S
	$(CPP) match.S > _match.s
	$(CC) -c _match.s
	mv _match.o match.o
	rm -f _match.s

match.lo: match.S
	$(CPP) match.S > _match.s
	$(CC) -c -fPIC _match.s
	mv _match.o match.lo
	rm -f _match.s

example.o: test/example.c zlib.h zconf.h
	$(CC) $(CFLAGS) -I. -c -o $@ test/example.c

minigzip.o: test/minigzip.c zlib.h zconf.h
	$(CC) $(CFLAGS) -I. -c -o $@ test/minigzip.c

example64.o: test/example.c zlib.h zconf.h
	$(CC) $(CFLAGS) -I. -D_FILE_OFFSET_BITS=64 -c -o $@ test/example.c

minigzip64.o: test/minigzip.c zlib.h zconf.h
	$(CC) $(CFLAGS) -I. -D_FILE_OFFSET_BITS=64 -c -o $@ test/minigzip.c

.SUFFIXES: .lo

.c.lo:
	-@mkdir objs 2>/dev/null || test -d objs
	$(CC) $(SFLAGS) -DPIC -c -o objs/$*.o $<
	-@mv objs/$*.o $@

placebo $(SHAREDLIBV): $(PIC_OBJS) libz.a
	$(LDSHARED) $(SFLAGS) -o $@ $(PIC_OBJS) $(LDSHAREDLIBC) $(LDFLAGS)
	rm -f $(SHAREDLIB) $(SHAREDLIBM)
	ln -s $@ $(SHAREDLIB)
	ln -s $@ $(SHAREDLIBM)
	-@rmdir objs

example$(EXE): example.o $(STATICLIB)
	$(CC) $(CFLAGS) -o $@ example.o $(TEST_LDFLAGS)

minigzip$(EXE): minigzip.o $(STATICLIB)
	$(CC) $(CFLAGS) -o $@ minigzip.o $(TEST_LDFLAGS)

examplesh$(EXE): example.o $(SHAREDLIBV)
	$(CC) $(CFLAGS) -o $@ example.o -L. $(SHAREDLIBV)

minigzipsh$(EXE): minigzip.o $(SHAREDLIBV)
	$(CC) $(CFLAGS) -o $@ minigzip.o -L. $(SHAREDLIBV)

example64$(EXE): example64.o $(STATICLIB)
	$(CC) $(CFLAGS) -o $@ example64.o $(TEST_LDFLAGS)

minigzip64$(EXE): minigzip64.o $(STATICLIB)
	$(CC) $(CFLAGS) -o $@ minigzip64.o $(TEST_LDFLAGS)

install-libs: $(LIBS)
	-@if [ ! -d $(DESTDIR)$(exec_prefix)  ]; then mkdir -p $(DESTDIR)$(exec_prefix); fi
	-@if [ ! -d $(DESTDIR)$(libdir)       ]; then mkdir -p $(DESTDIR)$(libdir); fi
	-@if [ ! -d $(DESTDIR)$(sharedlibdir) ]; then mkdir -p $(DESTDIR)$(sharedlibdir); fi
	-@if [ ! -d $(DESTDIR)$(man3dir)      ]; then mkdir -p $(DESTDIR)$(man3dir); fi
	-@if [ ! -d $(DESTDIR)$(pkgconfigdir) ]; then mkdir -p $(DESTDIR)$(pkgconfigdir); fi
	cp $(STATICLIB) $(DESTDIR)$(libdir)
	chmod 644 $(DESTDIR)$(libdir)/$(STATICLIB)
	-@($(RANLIB) $(DESTDIR)$(libdir)/libz.a || true) >/dev/null 2>&1
	-@if test -n "$(SHAREDLIBV)"; then \
	  cp $(SHAREDLIBV) $(DESTDIR)$(sharedlibdir); \
	  echo "cp $(SHAREDLIBV) $(DESTDIR)$(sharedlibdir)"; \
	  chmod 755 $(DESTDIR)$(sharedlibdir)/$(SHAREDLIBV); \
	  echo "chmod 755 $(DESTDIR)$(sharedlibdir)/$(SHAREDLIBV)"; \
	  rm -f $(DESTDIR)$(sharedlibdir)/$(SHAREDLIB) $(DESTDIR)$(sharedlibdir)/$(SHAREDLIBM); \
	  ln -s $(SHAREDLIBV) $(DESTDIR)$(sharedlibdir)/$(SHAREDLIB); \
	  ln -s $(SHAREDLIBV) $(DESTDIR)$(sharedlibdir)/$(SHAREDLIBM); \
	  ($(LDCONFIG) || true)  >/dev/null 2>&1; \
	fi
	cp zlib.3 $(DESTDIR)$(man3dir)
	chmod 644 $(DESTDIR)$(man3dir)/zlib.3
	cp zlib.pc $(DESTDIR)$(pkgconfigdir)
	chmod 644 $(DESTDIR)$(pkgconfigdir)/zlib.pc
# The ranlib in install is needed on NeXTSTEP which checks file times
# ldconfig is for Linux

install: install-libs
	-@if [ ! -d $(DESTDIR)$(includedir)   ]; then mkdir -p $(DESTDIR)$(includedir); fi
	cp zlib.h zconf.h $(DESTDIR)$(includedir)
	chmod 644 $(DESTDIR)$(includedir)/zlib.h $(DESTDIR)$(includedir)/zconf.h

uninstall:
	cd $(DESTDIR)$(includedir) && rm -f zlib.h zconf.h
	cd $(DESTDIR)$(libdir) && rm -f libz.a; \
	if test -n "$(SHAREDLIBV)" -a -f $(SHAREDLIBV); then \
	  rm -f $(SHAREDLIBV) $(SHAREDLIB) $(SHAREDLIBM); \
	fi
	cd $(DESTDIR)$(man3dir) && rm -f zlib.3
	cd $(DESTDIR)$(pkgconfigdir) && rm -f zlib.pc

docs: zlib.3.pdf

zlib.3.pdf: zlib.3
	groff -mandoc -f H -T ps zlib.3 | ps2pdf - zlib.3.pdf

zconf.h.cmakein: zconf.h.in
	-@ TEMPFILE=zconfh_$$; \
	echo "/#define ZCONF_H/ a\\\\\n#cmakedefine Z_PREFIX\\\\\n#cmakedefine Z_HAVE_UNISTD_H\n" >> $$TEMPFILE &&\
	sed -f $$TEMPFILE zconf.h.in > zconf.h.cmakein &&\
	touch -r zconf.h.in zconf.h.cmakein &&\
	rm $$TEMPFILE

zconf: zconf.h.in
	cp -p zconf.h.in zconf.h

mostlyclean: clean
clean:
	rm -f *.o *.lo *~ \
	   example$(EXE) minigzip$(EXE) examplesh$(EXE) minigzipsh$(EXE) \
	   example64$(EXE) minigzip64$(EXE) \
	   infcover \
	   libz.* foo.gz so_locations \
	   _match.s maketree contrib/infback9/*.o
	rm -rf objs
	rm -f *.gcda *.gcno *.gcov
	rm -f contrib/infback9/*.gcda contrib/infback9/*.gcno contrib/infback9/*.gcov

maintainer-clean: distclean
distclean: clean zconf zconf.h.cmakein docs
	rm -f Makefile zlib.pc configure.log
	-@rm -f .DS_Store
	-@printf 'all:\n\t-@echo "Please use ./configure first.  Thank you."\n' > Makefile
	-@printf '\ndistclean:\n\tmake -f Makefile.in distclean\n' >> Makefile
	-@touch -r Makefile.in Makefile

tags:
	etags *.[ch]

depend:
	makedepend -- $(CFLAGS) -- *.[ch]

# DO NOT DELETE THIS LINE -- make depend depends on it.

adler32.o zutil.o: zutil.h zlib.h zconf.h
gzclose.o gzlib.o gzread.o gzwrite.o: zlib.h zconf.h gzguts.h
compress.o example.o minigzip.o uncompr.o: zlib.h zconf.h
crc32.o: zutil.h zlib.h zconf.h crc32.h
deflate.o: deflate.h zutil.h zlib.h zconf.h
infback.o inflate.o: zutil.h zlib.h zconf.h inftrees.h inflate.h inffast.h inffixed.h
inffast.o: zutil.h zlib.h zconf.h inftrees.h inflate.h inffast.h
inftrees.o: zutil.h zlib.h zconf.h inftrees.h
trees.o: deflate.h zutil.h zlib.h zconf.h trees.h

adler32.lo zutil.lo: zutil.h zlib.h zconf.h
gzclose.lo gzlib.lo gzread.lo gzwrite.lo: zlib.h zconf.h gzguts.h
compress.lo example.lo minigzip.lo uncompr.lo: zlib.h zconf.h
crc32.lo: zutil.h zlib.h zconf.h crc32.h
deflate.lo: deflate.h zutil.h zlib.h zconf.h
infback.lo inflate.lo: zutil.h zlib.h zconf.h inftrees.h inflate.h inffast.h inffixed.h
inffast.lo: zutil.h zlib.h zconf.h inftrees.h inflate.h inffast.h
inftrees.lo: zutil.h zlib.h zconf.h inftrees.h
trees.lo: deflate.h zutil.h zlib.h zconf.h trees.h
