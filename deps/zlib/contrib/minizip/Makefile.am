lib_LTLIBRARIES = libminizip.la

if COND_DEMOS
bin_PROGRAMS = miniunzip minizip
endif

zlib_top_srcdir = $(top_srcdir)/../..
zlib_top_builddir = $(top_builddir)/../..

AM_CPPFLAGS = -I$(zlib_top_srcdir)
AM_LDFLAGS = -L$(zlib_top_builddir)

if WIN32
iowin32_src = iowin32.c
iowin32_h = iowin32.h
endif

libminizip_la_SOURCES = \
	ioapi.c \
	mztools.c \
	unzip.c \
	zip.c \
	${iowin32_src}

libminizip_la_LDFLAGS = $(AM_LDFLAGS) -version-info 1:0:0 -lz

minizip_includedir = $(includedir)/minizip
minizip_include_HEADERS = \
	crypt.h \
	ioapi.h \
	mztools.h \
	unzip.h \
	zip.h \
	${iowin32_h}

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = minizip.pc

EXTRA_PROGRAMS = miniunzip minizip

miniunzip_SOURCES = miniunz.c
miniunzip_LDADD = libminizip.la

minizip_SOURCES = minizip.c
minizip_LDADD = libminizip.la -lz
