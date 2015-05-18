## Process this with automake to create Makefile.in

AUTOMAKE_OPTIONS = foreign subdir-objects

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = include testsuite man

EXTRA_DIST = LICENSE ChangeLog.v1 ChangeLog.libgcj configure.host	\
	 src/aarch64/ffi.c src/aarch64/ffitarget.h src/aarch64/sysv.S	\
	 build-ios.sh src/alpha/ffi.c src/alpha/osf.S			\
	 src/alpha/ffitarget.h src/arm/ffi.c src/arm/sysv.S		\
	 src/arm/ffitarget.h src/avr32/ffi.c src/avr32/sysv.S		\
	 src/avr32/ffitarget.h src/cris/ffi.c src/cris/sysv.S		\
	 src/cris/ffitarget.h src/ia64/ffi.c src/ia64/ffitarget.h	\
	 src/ia64/ia64_flags.h src/ia64/unix.S src/mips/ffi.c		\
	 src/mips/n32.S src/mips/o32.S src/metag/ffi.c			\
	 src/metag/ffitarget.h src/metag/sysv.S src/moxie/ffi.c		\
	 src/moxie/ffitarget.h src/moxie/eabi.S src/mips/ffitarget.h	\
	 src/m32r/ffi.c src/m32r/sysv.S src/m32r/ffitarget.h		\
	 src/m68k/ffi.c src/m68k/sysv.S src/m68k/ffitarget.h		\
	 src/microblaze/ffi.c src/microblaze/sysv.S			\
	 src/microblaze/ffitarget.h src/powerpc/ffi.c			\
	 src/powerpc/sysv.S src/powerpc/linux64.S			\
	 src/powerpc/linux64_closure.S src/powerpc/ppc_closure.S	\
	 src/powerpc/asm.h src/powerpc/aix.S src/powerpc/darwin.S	\
	 src/powerpc/aix_closure.S src/powerpc/darwin_closure.S		\
	 src/powerpc/ffi_darwin.c src/powerpc/ffitarget.h		\
	 src/s390/ffi.c src/s390/sysv.S src/s390/ffitarget.h		\
	 src/sh/ffi.c src/sh/sysv.S src/sh/ffitarget.h src/sh64/ffi.c	\
	 src/sh64/sysv.S src/sh64/ffitarget.h src/sparc/v8.S		\
	 src/sparc/v9.S src/sparc/ffitarget.h src/sparc/ffi.c		\
	 src/x86/darwin64.S src/x86/ffi.c src/x86/sysv.S		\
	 src/x86/win32.S src/x86/darwin.S src/x86/win64.S		\
	 src/x86/freebsd.S src/x86/ffi64.c src/x86/unix64.S		\
	 src/x86/ffitarget.h src/pa/ffitarget.h src/pa/ffi.c		\
	 src/pa/linux.S src/pa/hpux32.S src/frv/ffi.c src/bfin/ffi.c	\
	 src/bfin/ffitarget.h src/bfin/sysv.S src/frv/eabi.S		\
	 src/frv/ffitarget.h src/dlmalloc.c src/tile/ffi.c		\
	 src/tile/ffitarget.h src/tile/tile.S libtool-version		\
	 src/xtensa/ffitarget.h src/xtensa/ffi.c src/xtensa/sysv.S	\
	 ChangeLog.libffi m4/libtool.m4 m4/lt~obsolete.m4		\
	 m4/ltoptions.m4 m4/ltsugar.m4 m4/ltversion.m4			\
	 m4/ltversion.m4 src/arm/gentramp.sh src/debug.c msvcc.sh	\
	 generate-ios-source-and-headers.py				\
	 generate-osx-source-and-headers.py				\
	 libffi.xcodeproj/project.pbxproj src/arm/trampoline.S		\
	 libtool-ldflags

info_TEXINFOS = doc/libffi.texi

## ################################################################

##
## This section is for make and multilib madness.
##

# Work around what appears to be a GNU make bug handling MAKEFLAGS
# values defined in terms of make variables, as is the case for CC and
# friends when we are called from the top level Makefile.
AM_MAKEFLAGS = \
	"AR_FLAGS=$(AR_FLAGS)" \
	"CC_FOR_BUILD=$(CC_FOR_BUILD)" \
	"CFLAGS=$(CFLAGS)" \
	"CXXFLAGS=$(CXXFLAGS)" \
	"CFLAGS_FOR_BUILD=$(CFLAGS_FOR_BUILD)" \
	"CFLAGS_FOR_TARGET=$(CFLAGS_FOR_TARGET)" \
	"INSTALL=$(INSTALL)" \
	"INSTALL_DATA=$(INSTALL_DATA)" \
	"INSTALL_PROGRAM=$(INSTALL_PROGRAM)" \
	"INSTALL_SCRIPT=$(INSTALL_SCRIPT)" \
	"JC1FLAGS=$(JC1FLAGS)" \
	"LDFLAGS=$(LDFLAGS)" \
	"LIBCFLAGS=$(LIBCFLAGS)" \
	"LIBCFLAGS_FOR_TARGET=$(LIBCFLAGS_FOR_TARGET)" \
	"MAKE=$(MAKE)" \
	"MAKEINFO=$(MAKEINFO) $(MAKEINFOFLAGS)" \
	"PICFLAG=$(PICFLAG)" \
	"PICFLAG_FOR_TARGET=$(PICFLAG_FOR_TARGET)" \
	"RUNTESTFLAGS=$(RUNTESTFLAGS)" \
	"SHELL=$(SHELL)" \
	"exec_prefix=$(exec_prefix)" \
	"infodir=$(infodir)" \
	"libdir=$(libdir)" \
	"mandir=$(mandir)" \
	"prefix=$(prefix)" \
	"AR=$(AR)" \
	"AS=$(AS)" \
	"CC=$(CC)" \
	"CXX=$(CXX)" \
	"LD=$(LD)" \
	"NM=$(NM)" \
	"RANLIB=$(RANLIB)" \
	"DESTDIR=$(DESTDIR)"

# Subdir rules rely on $(FLAGS_TO_PASS)
FLAGS_TO_PASS = $(AM_MAKEFLAGS)

MAKEOVERRIDES=

toolexeclib_LTLIBRARIES = libffi.la
noinst_LTLIBRARIES = libffi_convenience.la

libffi_la_SOURCES = src/prep_cif.c src/types.c \
		src/raw_api.c src/java_raw_api.c src/closures.c

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libffi.pc

nodist_libffi_la_SOURCES =

if FFI_DEBUG
nodist_libffi_la_SOURCES += src/debug.c
endif

if MIPS
nodist_libffi_la_SOURCES += src/mips/ffi.c src/mips/o32.S src/mips/n32.S
endif
if BFIN
nodist_libffi_la_SOURCES += src/bfin/ffi.c src/bfin/sysv.S
endif
if X86
nodist_libffi_la_SOURCES += src/x86/ffi.c src/x86/sysv.S
endif
if X86_FREEBSD
nodist_libffi_la_SOURCES += src/x86/ffi.c src/x86/freebsd.S
endif
if X86_WIN32
nodist_libffi_la_SOURCES += src/x86/ffi.c src/x86/win32.S
endif
if X86_WIN64
nodist_libffi_la_SOURCES += src/x86/ffi.c src/x86/win64.S
endif
if X86_DARWIN
nodist_libffi_la_SOURCES += src/x86/ffi.c src/x86/darwin.S src/x86/ffi64.c src/x86/darwin64.S
endif
if SPARC
nodist_libffi_la_SOURCES += src/sparc/ffi.c src/sparc/v8.S src/sparc/v9.S
endif
if ALPHA
nodist_libffi_la_SOURCES += src/alpha/ffi.c src/alpha/osf.S
endif
if IA64
nodist_libffi_la_SOURCES += src/ia64/ffi.c src/ia64/unix.S
endif
if M32R
nodist_libffi_la_SOURCES += src/m32r/sysv.S src/m32r/ffi.c
endif
if M68K
nodist_libffi_la_SOURCES += src/m68k/ffi.c src/m68k/sysv.S
endif
if MOXIE
nodist_libffi_la_SOURCES += src/moxie/ffi.c src/moxie/eabi.S
endif
if MICROBLAZE
nodist_libffi_la_SOURCES += src/microblaze/ffi.c src/microblaze/sysv.S
endif
if POWERPC
nodist_libffi_la_SOURCES += src/powerpc/ffi.c src/powerpc/sysv.S src/powerpc/ppc_closure.S src/powerpc/linux64.S src/powerpc/linux64_closure.S
endif
if POWERPC_AIX
nodist_libffi_la_SOURCES += src/powerpc/ffi_darwin.c src/powerpc/aix.S src/powerpc/aix_closure.S
endif
if POWERPC_DARWIN
nodist_libffi_la_SOURCES += src/powerpc/ffi_darwin.c src/powerpc/darwin.S src/powerpc/darwin_closure.S
endif
if POWERPC_FREEBSD
nodist_libffi_la_SOURCES += src/powerpc/ffi.c src/powerpc/sysv.S src/powerpc/ppc_closure.S
endif
if AARCH64
nodist_libffi_la_SOURCES += src/aarch64/sysv.S src/aarch64/ffi.c
endif
if ARM
nodist_libffi_la_SOURCES += src/arm/sysv.S src/arm/ffi.c
if FFI_EXEC_TRAMPOLINE_TABLE
nodist_libffi_la_SOURCES += src/arm/trampoline.S
endif
endif
if AVR32
nodist_libffi_la_SOURCES += src/avr32/sysv.S src/avr32/ffi.c
endif
if LIBFFI_CRIS
nodist_libffi_la_SOURCES += src/cris/sysv.S src/cris/ffi.c
endif
if FRV
nodist_libffi_la_SOURCES += src/frv/eabi.S src/frv/ffi.c
endif
if S390
nodist_libffi_la_SOURCES += src/s390/sysv.S src/s390/ffi.c
endif
if X86_64
nodist_libffi_la_SOURCES += src/x86/ffi64.c src/x86/unix64.S src/x86/ffi.c src/x86/sysv.S
endif
if SH
nodist_libffi_la_SOURCES += src/sh/sysv.S src/sh/ffi.c
endif
if SH64
nodist_libffi_la_SOURCES += src/sh64/sysv.S src/sh64/ffi.c
endif
if PA_LINUX
nodist_libffi_la_SOURCES += src/pa/linux.S src/pa/ffi.c
endif
if PA_HPUX
nodist_libffi_la_SOURCES += src/pa/hpux32.S src/pa/ffi.c
endif
if TILE
nodist_libffi_la_SOURCES += src/tile/tile.S src/tile/ffi.c
endif
if XTENSA
nodist_libffi_la_SOURCES += src/xtensa/sysv.S src/xtensa/ffi.c
endif
if METAG
nodist_libffi_la_SOURCES += src/metag/sysv.S src/metag/ffi.c
endif

libffi_convenience_la_SOURCES = $(libffi_la_SOURCES)
nodist_libffi_convenience_la_SOURCES = $(nodist_libffi_la_SOURCES)

LTLDFLAGS = $(shell $(SHELL) $(top_srcdir)/libtool-ldflags $(LDFLAGS))

libffi_la_LDFLAGS = -no-undefined -version-info `grep -v '^\#' $(srcdir)/libtool-version` $(LTLDFLAGS) $(AM_LTLDFLAGS)

AM_CPPFLAGS = -I. -I$(top_srcdir)/include -Iinclude -I$(top_srcdir)/src
AM_CCASFLAGS = $(AM_CPPFLAGS)

