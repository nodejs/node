%define _unpackaged_files_terminate_build 0
%define debug_package %{nil}

Release: 1

%define openssldir /var/ssl

Summary: Secure Sockets Layer and cryptography libraries and tools
Name: openssl
Version: 1.0.2k
Source0: ftp://ftp.openssl.org/source/%{name}-%{version}.tar.gz
License: OpenSSL
Group: System Environment/Libraries
Provides: SSL
URL: http://www.openssl.org/
Packager: Damien Miller <djm@mindrot.org>
BuildRoot:   /var/tmp/%{name}-%{version}-root

%description
The OpenSSL Project is a collaborative effort to develop a robust,
commercial-grade, fully featured, and Open Source toolkit implementing the
Secure Sockets Layer (SSL v2/v3) and Transport Layer Security (TLS v1)
protocols as well as a full-strength general purpose cryptography library.
The project is managed by a worldwide community of volunteers that use the
Internet to communicate, plan, and develop the OpenSSL tookit and its related
documentation. 

OpenSSL is based on the excellent SSLeay library developed from Eric A.
Young and Tim J. Hudson.  The OpenSSL toolkit is licensed under an
Apache-style licence, which basically means that you are free to get and
use it for commercial and non-commercial purposes. 

This package contains the base OpenSSL cryptography and SSL/TLS 
libraries and tools.

%package devel
Summary: Secure Sockets Layer and cryptography static libraries and headers
Group: Development/Libraries
Requires: openssl
%description devel
The OpenSSL Project is a collaborative effort to develop a robust,
commercial-grade, fully featured, and Open Source toolkit implementing the
Secure Sockets Layer (SSL v2/v3) and Transport Layer Security (TLS v1)
protocols as well as a full-strength general purpose cryptography library.
The project is managed by a worldwide community of volunteers that use the
Internet to communicate, plan, and develop the OpenSSL tookit and its related
documentation. 

OpenSSL is based on the excellent SSLeay library developed from Eric A.
Young and Tim J. Hudson.  The OpenSSL toolkit is licensed under an
Apache-style licence, which basically means that you are free to get and
use it for commercial and non-commercial purposes. 

This package contains the the OpenSSL cryptography and SSL/TLS 
static libraries and header files required when developing applications.

%package doc
Summary: OpenSSL miscellaneous files
Group: Documentation
Requires: openssl
%description doc
The OpenSSL Project is a collaborative effort to develop a robust,
commercial-grade, fully featured, and Open Source toolkit implementing the
Secure Sockets Layer (SSL v2/v3) and Transport Layer Security (TLS v1)
protocols as well as a full-strength general purpose cryptography library.
The project is managed by a worldwide community of volunteers that use the
Internet to communicate, plan, and develop the OpenSSL tookit and its related
documentation. 

OpenSSL is based on the excellent SSLeay library developed from Eric A.
Young and Tim J. Hudson.  The OpenSSL toolkit is licensed under an
Apache-style licence, which basically means that you are free to get and
use it for commercial and non-commercial purposes. 

This package contains the the OpenSSL cryptography and SSL/TLS extra
documentation and POD files from which the man pages were produced.

%prep

%setup -q

%build 

%define CONFIG_FLAGS -DSSL_ALLOW_ADH --prefix=%{_exec_prefix} --openssldir=%{openssldir}

perl util/perlpath.pl /usr/bin/perl

%ifarch i386 i486 i586 i686
./Configure %{CONFIG_FLAGS} linux-elf shared
%endif
%ifarch ppc
./Configure %{CONFIG_FLAGS} linux-ppc shared
%endif
%ifarch alpha
./Configure %{CONFIG_FLAGS} linux-alpha shared
%endif
%ifarch x86_64
./Configure %{CONFIG_FLAGS} linux-x86_64 shared
%endif
LD_LIBRARY_PATH=`pwd` make
LD_LIBRARY_PATH=`pwd` make rehash
LD_LIBRARY_PATH=`pwd` make test

%install
rm -rf $RPM_BUILD_ROOT
make MANDIR=%{_mandir} MANSUFFIX=ssl INSTALL_PREFIX="$RPM_BUILD_ROOT" install

# Make backwards-compatibility symlink to ssleay
ln -sf /usr/bin/openssl $RPM_BUILD_ROOT/usr/bin/ssleay

%clean
rm -rf $RPM_BUILD_ROOT

%files 
%defattr(0644,root,root,0755)
%doc CHANGES CHANGES.SSLeay LICENSE NEWS README

%attr(0755,root,root) %{_bindir}/*
%attr(0755,root,root) %{_libdir}/*.so*
%attr(0755,root,root) %{_libdir}/engines/*.so*
%attr(0755,root,root) %{_libdir}/pkgconfig/*
%attr(0755,root,root) %{openssldir}/misc/*
%attr(0644,root,root) %{_mandir}/man[157]/*

%config %attr(0644,root,root) %{openssldir}/openssl.cnf 
%dir %attr(0755,root,root) %{openssldir}/certs
%dir %attr(0755,root,root) %{openssldir}/misc
%dir %attr(0750,root,root) %{openssldir}/private

%files devel
%defattr(0644,root,root,0755)
%doc CHANGES CHANGES.SSLeay LICENSE NEWS README

%attr(0644,root,root) %{_libdir}/*.a
%attr(0644,root,root) %{_libdir}/pkgconfig/openssl.pc
%attr(0644,root,root) %{_includedir}/openssl/*
%attr(0644,root,root) %{_mandir}/man[3]/*

%files doc
%defattr(0644,root,root,0755)
%doc CHANGES CHANGES.SSLeay LICENSE NEWS README
%doc doc

%post
ldconfig

%postun
ldconfig

%changelog
* Sun Jun  6 2005 Richard Levitte <richard@levitte.org>
- Remove the incorrect installation of '%{openssldir}/lib'.
* Wed May  7 2003 Richard Levitte <richard@levitte.org>
- Add /usr/lib/pkgconfig/openssl.pc to the development section.
* Thu Mar 22 2001 Richard Levitte <richard@levitte.org>
- Removed redundant subsection that re-installed libcrypto.a and libssl.a
  as well.  Also remove RSAref stuff completely, since it's not needed
  any more.
* Thu Mar 15 2001 Jeremiah Johnson <jjohnson@penguincomputing.com>
- Removed redundant subsection that re-installed libcrypto.so.0.9.6 and
  libssl.so.0.9.6.  As well as the subsection that created symlinks for
  these.  make install handles all this.
* Sat Oct 21 2000 Horms <horms@vergenet.net>
- Make sure symlinks are created by using -f flag to ln.
  Otherwise some .so libraries are copied rather than
  linked in the resulting binary RPM. This causes the package
  to be larger than neccessary and makes ldconfig complain.
* Fri Oct 13 2000 Horms <horms@vergenet.net>
- Make defattr is set for files in all packages so packages built as
  non-root will still be installed with files owned by root.
* Thu Sep 14 2000 Richard Levitte <richard@levitte.org>
- Changed to adapt to the new (supported) way of making shared libraries
- Installs all static libraries, not just libRSAglue.a
- Extra documents now end up in a separate document package
* Sun Feb 27 2000 Damien Miller <djm@mindrot.org>
- Merged patches to spec
- Updated to 0.9.5beta2 (now with manpages)
* Sat Feb  5 2000 Michal Jaegermann <michal@harddata.com>
- added 'linux-alpha' to configuration
- fixed nasty absolute links
* Tue Jan 25 2000 Bennett Todd <bet@rahul.net>
- Added -DSSL_ALLOW_ADH, bumped Release to 4
* Thu Oct 14 1999 Damien Miller <djm@mindrot.org>
- Set default permissions
- Removed documentation from devel sub-package
* Thu Sep 30 1999 Damien Miller <djm@mindrot.org>
- Added "make test" stage
- GPG signed
* Tue Sep 10 1999 Damien Miller <damien@ibs.com.au>
- Updated to version 0.9.4
* Tue May 25 1999 Damien Miller <damien@ibs.com.au>
- Updated to version 0.9.3
- Added attributes for all files
- Paramatised openssl directory
* Sat Mar 20 1999 Carlo M. Arenas Belon <carenas@jmconsultores.com.pe>
- Added "official" bnrec patch and taking other out
- making a link from ssleay to openssl binary
- putting all changelog together on SPEC file
* Fri Mar  5 1999 Henri Gomez <gomez@slib.fr>
- Added bnrec patch
* Tue Dec 29 1998 Jonathan Ruano <kobalt@james.encomix.es>
- minimum spec and patches changes for openssl
- modified for openssl sources
* Sat Aug  8 1998 Khimenko Victor <khim@sch57.msk.ru>
- shared library creating process honours $RPM_OPT_FLAGS
- shared libarry supports threads (as well as static library)
* Wed Jul 22 1998 Khimenko Victor <khim@sch57.msk.ru>
- building of shared library completely reworked
* Tue Jul 21 1998 Khimenko Victor <khim@sch57.msk.ru>
- RPM is BuildRoot'ed
* Tue Feb 10 1998 Khimenko Victor <khim@sch57.msk.ru>
- all stuff is moved out of /usr/local
