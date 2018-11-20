# Makefile for zlib, derived from Makefile.dj2.
# Modified for mingw32 by C. Spieler, 6/16/98.
# Updated for zlib 1.2.x by Christian Spieler and Cosmin Truta, Mar-2003.
# Last updated: Mar 2012.
# Tested under Cygwin and MinGW.

# Copyright (C) 1995-2003 Jean-loup Gailly.
# For conditions of distribution and use, see copyright notice in zlib.h

# To compile, or to compile and test, type from the top level zlib directory:
#
#   make -fwin32/Makefile.gcc;  make test testdll -fwin32/Makefile.gcc
#
# To use the asm code, type:
#   cp contrib/asm?86/match.S ./match.S
#   make LOC=-DASMV OBJA=match.o -fwin32/Makefile.gcc
#
# To install libz.a, zconf.h and zlib.h in the system directories, type:
#
#   make install -fwin32/Makefile.gcc
#
# BINARY_PATH, INCLUDE_PATH and LIBRARY_PATH must be set.
#
# To install the shared lib, append SHARED_MODE=1 to the make command :
#
#   make install -fwin32/Makefile.gcc SHARED_MODE=1

# Note:
# If the platform is *not* MinGW (e.g. it is Cygwin or UWIN),
# the DLL name should be changed from "zlib1.dll".

STATICLIB = libz.a
SHAREDLIB = zlib1.dll
IMPLIB    = libz.dll.a

#
# Set to 1 if shared object needs to be installed
#
SHARED_MODE=0

#LOC = -DASMV
#LOC = -DZLIB_DEBUG -g

PREFIX =
CC = $(PREFIX)gcc
CFLAGS = $(LOC) -O3 -Wall

AS = $(CC)
ASFLAGS = $(LOC) -Wall

LD = $(CC)
LDFLAGS = $(LOC)

AR = $(PREFIX)ar
ARFLAGS = rcs

RC = $(PREFIX)windres
RCFLAGS = --define GCC_WINDRES

STRIP = $(PREFIX)strip

CP = cp -fp
# If GNU install is available, replace $(CP) with install.
INSTALL = $(CP)
RM = rm -f

prefix ?= /usr/local
exec_prefix = $(prefix)

OBJS = adler32.o compress.o crc32.o deflate.o gzclose.o gzlib.o gzread.o \
       gzwrite.o infback.o inffast.o inflate.o inftrees.o trees.o uncompr.o zutil.o
OBJA =

all: $(STATICLIB) $(SHAREDLIB) $(IMPLIB) example.exe minigzip.exe example_d.exe minigzip_d.exe

test: example.exe minigzip.exe
	./example
	echo hello world | ./minigzip | ./minigzip -d

testdll: example_d.exe minigzip_d.exe
	./example_d
	echo hello world | ./minigzip_d | ./minigzip_d -d

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.S.o:
	$(AS) $(ASFLAGS) -c -o $@ $<

$(STATICLIB): $(OBJS) $(OBJA)
	$(AR) $(ARFLAGS) $@ $(OBJS) $(OBJA)

$(IMPLIB): $(SHAREDLIB)

$(SHAREDLIB): win32/zlib.def $(OBJS) $(OBJA) zlibrc.o
	$(CC) -shared -Wl,--out-implib,$(IMPLIB) $(LDFLAGS) \
	-o $@ win32/zlib.def $(OBJS) $(OBJA) zlibrc.o
	$(STRIP) $@

example.exe: example.o $(STATICLIB)
	$(LD) $(LDFLAGS) -o $@ example.o $(STATICLIB)
	$(STRIP) $@

minigzip.exe: minigzip.o $(STATICLIB)
	$(LD) $(LDFLAGS) -o $@ minigzip.o $(STATICLIB)
	$(STRIP) $@

example_d.exe: example.o $(IMPLIB)
	$(LD) $(LDFLAGS) -o $@ example.o $(IMPLIB)
	$(STRIP) $@

minigzip_d.exe: minigzip.o $(IMPLIB)
	$(LD) $(LDFLAGS) -o $@ minigzip.o $(IMPLIB)
	$(STRIP) $@

example.o: test/example.c zlib.h zconf.h
	$(CC) $(CFLAGS) -I. -c -o $@ test/example.c

minigzip.o: test/minigzip.c zlib.h zconf.h
	$(CC) $(CFLAGS) -I. -c -o $@ test/minigzip.c

zlibrc.o: win32/zlib1.rc
	$(RC) $(RCFLAGS) -o $@ win32/zlib1.rc

.PHONY: install uninstall clean

install: zlib.h zconf.h $(STATICLIB) $(IMPLIB)
	@if test -z "$(DESTDIR)$(INCLUDE_PATH)" -o -z "$(DESTDIR)$(LIBRARY_PATH)" -o -z "$(DESTDIR)$(BINARY_PATH)"; then \
		echo INCLUDE_PATH, LIBRARY_PATH, and BINARY_PATH must be specified; \
		exit 1; \
	fi
	-@mkdir -p '$(DESTDIR)$(INCLUDE_PATH)'
	-@mkdir -p '$(DESTDIR)$(LIBRARY_PATH)' '$(DESTDIR)$(LIBRARY_PATH)'/pkgconfig
	-if [ "$(SHARED_MODE)" = "1" ]; then \
		mkdir -p '$(DESTDIR)$(BINARY_PATH)'; \
		$(INSTALL) $(SHAREDLIB) '$(DESTDIR)$(BINARY_PATH)'; \
		$(INSTALL) $(IMPLIB) '$(DESTDIR)$(LIBRARY_PATH)'; \
	fi
	-$(INSTALL) zlib.h '$(DESTDIR)$(INCLUDE_PATH)'
	-$(INSTALL) zconf.h '$(DESTDIR)$(INCLUDE_PATH)'
	-$(INSTALL) $(STATICLIB) '$(DESTDIR)$(LIBRARY_PATH)'
	sed \
		-e 's|@prefix@|${prefix}|g' \
		-e 's|@exec_prefix@|${exec_prefix}|g' \
		-e 's|@libdir@|$(LIBRARY_PATH)|g' \
		-e 's|@sharedlibdir@|$(LIBRARY_PATH)|g' \
		-e 's|@includedir@|$(INCLUDE_PATH)|g' \
		-e 's|@VERSION@|'`sed -n -e '/VERSION "/s/.*"\(.*\)".*/\1/p' zlib.h`'|g' \
		zlib.pc.in > '$(DESTDIR)$(LIBRARY_PATH)'/pkgconfig/zlib.pc

uninstall:
	-if [ "$(SHARED_MODE)" = "1" ]; then \
		$(RM) '$(DESTDIR)$(BINARY_PATH)'/$(SHAREDLIB); \
		$(RM) '$(DESTDIR)$(LIBRARY_PATH)'/$(IMPLIB); \
	fi
	-$(RM) '$(DESTDIR)$(INCLUDE_PATH)'/zlib.h
	-$(RM) '$(DESTDIR)$(INCLUDE_PATH)'/zconf.h
	-$(RM) '$(DESTDIR)$(LIBRARY_PATH)'/$(STATICLIB)

clean:
	-$(RM) $(STATICLIB)
	-$(RM) $(SHAREDLIB)
	-$(RM) $(IMPLIB)
	-$(RM) *.o
	-$(RM) *.exe
	-$(RM) foo.gz

adler32.o: zlib.h zconf.h
compress.o: zlib.h zconf.h
crc32.o: crc32.h zlib.h zconf.h
deflate.o: deflate.h zutil.h zlib.h zconf.h
gzclose.o: zlib.h zconf.h gzguts.h
gzlib.o: zlib.h zconf.h gzguts.h
gzread.o: zlib.h zconf.h gzguts.h
gzwrite.o: zlib.h zconf.h gzguts.h
inffast.o: zutil.h zlib.h zconf.h inftrees.h inflate.h inffast.h
inflate.o: zutil.h zlib.h zconf.h inftrees.h inflate.h inffast.h
infback.o: zutil.h zlib.h zconf.h inftrees.h inflate.h inffast.h
inftrees.o: zutil.h zlib.h zconf.h inftrees.h
trees.o: deflate.h zutil.h zlib.h zconf.h trees.h
uncompr.o: zlib.h zconf.h
zutil.o: zutil.h zlib.h zconf.h
