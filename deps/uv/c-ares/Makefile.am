AUTOMAKE_OPTIONS = foreign nostdinc

ACLOCAL_AMFLAGS = -I m4

# Specify our include paths here, and do it relative to $(top_srcdir) and
# $(top_builddir), to ensure that these paths which belong to the library
# being currently built and tested are searched before the library which
# might possibly already be installed in the system.
#
# When using the low-level hard-hacking memory leak tracking code from
# libcurl the generated curl/curlbuild.h file must also be reachable.
# Using the libcurl lowlevel code from within c-ares library is ugly and
# only works when c-ares is built and linked with a similarly debug-build
# libcurl, but we do this anyway for convenience.
#
# $(top_builddir)/../include/curl for generated curlbuild.h included from curl.h
# $(top_builddir)/../include is for libcurl's generated curl/curlbuild.h file
# $(top_srcdir)/../include is for libcurl's external include files
# $(top_builddir)/../lib is for libcurl's generated lib/curl_config.h file
# $(top_srcdir)/../lib is for libcurl's lib/setup.h and other "private" files
# $(top_builddir) is for c-ares's generated ares_config.h file
# $(top_srcdir) is for c-ares's ares_setup.h and other "c-ares-private" files

if CURLDEBUG
INCLUDES = -I$(top_builddir)/../include/curl \
           -I$(top_builddir)/../include      \
           -I$(top_srcdir)/../include        \
           -I$(top_builddir)/../lib          \
           -I$(top_srcdir)/../lib            \
           -I$(top_builddir)                 \
           -I$(top_srcdir)
else
INCLUDES = -I$(top_builddir) \
           -I$(top_srcdir)
endif

lib_LTLIBRARIES = libcares.la

man_MANS = $(MANPAGES)

MSVCFILES = vc/vc6aws.dsw vc/acountry/vc6acountry.dsp vc/adig/vc6adig.dsp \
 vc/ahost/vc6ahost.dsp vc/cares/vc6cares.dsp vc/cares/vc6cares.dsw

if CURLDEBUG
PROGS =
else
PROGS = ahost adig acountry
endif

noinst_PROGRAMS =$(PROGS)

# adig and ahost are just sample programs and thus not mentioned with the
# regular sources and headers
EXTRA_DIST = AUTHORS CHANGES README.cares Makefile.inc Makefile.dj	   \
 Makefile.m32 Makefile.netware Makefile.msvc Makefile.Watcom $(man_MANS)   \
 config-win32.h RELEASE-NOTES libcares.pc.in buildconf get_ver.awk maketgz \
 TODO ares_build.h.in $(PDFPAGES) cares.rc README.msvc $(MSVCFILES)	   \
 $(CSOURCES) $(HHEADERS) config-dos.h

CLEANFILES = $(PDFPAGES) $(HTMLPAGES)

DISTCLEANFILES = ares_build.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcares.pc

VER=-version-info 2:0:0
# This flag accepts an argument of the form current[:revision[:age]]. So,
# passing -version-info 3:12:1 sets current to 3, revision to 12, and age to
# 1.
#
# If either revision or age are omitted, they default to 0. Also note that age
# must be less than or equal to the current interface number.
#
# Here are a set of rules to help you update your library version information: 
#
# 1.Start with version information of 0:0:0 for each libtool library.
#
# 2.Update the version information only immediately before a public release of
# your software. More frequent updates are unnecessary, and only guarantee
# that the current interface number gets larger faster.
#
# 3.If the library source code has changed at all since the last update, then
# increment revision (c:r+1:a)
#
# 4.If any interfaces have been added, removed, or changed since the last
# update, increment current, and set revision to 0. (c+1:r=0:a)
#
# 5.If any interfaces have been added since the last public release, then
# increment age. (c:r:a+1)
#
# 6.If any interfaces have been removed since the last public release, then
# set age to 0. (c:r:a=0)
#

if NO_UNDEFINED
# The -no-undefined flag is crucial for this to build fine on some platforms
UNDEF = -no-undefined
endif

libcares_la_LDFLAGS = $(UNDEF) $(VER)

# Add -Werror if defined
CFLAGS += @CARES_CFLAG_EXTRAS@

# Makefile.inc provides the CSOURCES and HHEADERS defines
include Makefile.inc

libcares_la_SOURCES = $(CSOURCES) $(HHEADERS)

# where to install the c-ares headers
libcares_ladir = $(includedir)
# what headers to install on 'make install':
libcares_la_HEADERS = ares.h ares_version.h ares_dns.h \
	ares_build.h ares_rules.h

ahost_SOURCES = ahost.c $(SAMPLESOURCES) $(SAMPLEHEADERS)
ahost_LDADD = $(top_builddir)/libcares.la
ahost_CFLAGS = $(AM_CFLAGS)

adig_SOURCES = adig.c $(SAMPLESOURCES) $(SAMPLEHEADERS)
adig_LDADD = $(top_builddir)/libcares.la
adig_CFLAGS = $(AM_CFLAGS)

acountry_SOURCES = acountry.c $(SAMPLESOURCES) $(SAMPLEHEADERS)
acountry_LDADD = $(top_builddir)/libcares.la
acountry_CFLAGS = $(AM_CFLAGS)

SOURCEDMANDIR = man3
SOURCEDMANPAGES = ares_init.3

clean-local: clean-sourced-manpages

clean-sourced-manpages:
	@srcdmandir='$(SOURCEDMANDIR)'; \
	echo "rm -rf $(top_builddir)/$$srcdmandir"; \
	rm -rf $(top_builddir)/$$srcdmandir

sourced-manpages: clean-sourced-manpages
	@srcdmandir='$(SOURCEDMANDIR)'; \
	srcdmanfiles='$(SOURCEDMANPAGES)'; \
	mkdir $(top_builddir)/$$srcdmandir; \
	for file in $$srcdmanfiles; do \
	  if test -f $(top_srcdir)/$$file; then \
	    echo "cp $(top_srcdir)/$$file $(top_builddir)/$$srcdmandir/$$file"; \
	    cp $(top_srcdir)/$$file $(top_builddir)/$$srcdmandir/$$file; \
	  fi; \
	done

MAN2HTML = roffit --mandir=. < $< >$@

SUFFIXES = .3 .html

html: sourced-manpages $(HTMLPAGES)

.3.html:
	$(MAN2HTML)

pdf: sourced-manpages $(PDFPAGES)

.3.pdf:
	@(foo=`echo $@ | sed -e 's/\.[0-9]$$//g'`; \
	groff -Tps -man $< >$$foo.ps; \
	ps2pdf $$foo.ps $@; \
	rm $$foo.ps; \
	echo "converted $< to $@")

# Make files named *.dist replace the file without .dist extension
dist-hook:
	find $(distdir) -name "*.dist" -exec rm {} \;
	(distit=`find $(srcdir) -name "*.dist"`; \
	for file in $$distit; do \
	  strip=`echo $$file | sed -e s/^$(srcdir)// -e s/\.dist//`; \
	  cp $$file $(distdir)$$strip; \
	done)
