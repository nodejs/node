AUTOMAKE_OPTIONS = foreign nostdinc 1.9.6
ACLOCAL_AMFLAGS = -I m4 --install

MSVCFILES = msvc_ver.inc buildconf.bat

# adig and ahost are just sample programs and thus not mentioned with the
# regular sources and headers
EXTRA_DIST = AUTHORS CHANGES README.cares $(man_MANS) RELEASE-NOTES	\
 c-ares-config.cmake.in libcares.pc.cmake libcares.pc.in buildconf get_ver.awk \
 maketgz TODO README.msvc $(MSVCFILES) INSTALL.md README.md LICENSE.md \
 CMakeLists.txt Makefile.dj Makefile.m32 Makefile.netware Makefile.msvc \
 Makefile.Watcom AUTHORS CONTRIBUTING.md SECURITY.md TODO


CLEANFILES = $(PDFPAGES) $(HTMLPAGES)

DISTCLEANFILES = include/ares_build.h

DIST_SUBDIRS = include src test docs

SUBDIRS = @BUILD_SUBDIRS@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcares.pc

# where to install the c-ares headers
libcares_ladir = $(includedir)


# Make files named *.dist replace the file without .dist extension
dist-hook:
	find $(distdir) -name "*.dist" -exec rm {} \;
	(distit=`find $(srcdir) -name "*.dist"`; \
	for file in $$distit; do \
	  strip=`echo $$file | sed -e s/^$(srcdir)// -e s/\.dist//`; \
	  cp $$file $(distdir)$$strip; \
	done)
