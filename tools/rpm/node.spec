# Copyright (c) 2013, StrongLoop, Inc. <callback@strongloop.com>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# This is updated by rpmbuild.sh.
%define _version 0.10.12

Name: node
Version: %{_version}
Release: 1
Summary: Node.js is a platform for building fast, scalable network applications.
Group: Development/Languages
License: MIT
URL: https://nodejs.org/
Source0: https://nodejs.org/dist/v%{_version}/node-v%{_version}.tar.gz
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: glibc-devel
BuildRequires: make
BuildRequires: python

# Conflicts with the HAM radio package.
Conflicts: node <= 0.3.2-11

# Conflicts with the Fedora node.js package.
Conflicts: nodejs


%description
Node.js is a platform built on Chrome's JavaScript runtime for easily
building fast, scalable network applications.

Node.js uses an event-driven, non-blocking I/O model that makes it
lightweight and efficient, perfect for data-intensive real-time
applications that run across distributed devices.


%prep
%setup -q


%build
%ifarch arm
%define _dest_cpu arm
%endif

%ifarch i386 i686
%define _dest_cpu ia32
%endif

%ifarch x86_64
%define _dest_cpu x64
%endif

./configure --prefix=/usr --dest-cpu=%{_dest_cpu}
make %{?_smp_mflags}


%check
#make test


# Use mildly hard-coded paths in the install and files targets for now.
# _libdir is /usr/lib64 on some systems but the installer always installs
# to /usr/lib.  I have commits sitting in a branch that add --libdir and
# --mandir configure switches to the configure script but it's debatable
# if it's worth the extra complexity.
%install
export DONT_STRIP=1  # Don't strip debug symbols for now.
make install DESTDIR=%{buildroot}
rm -fr %{buildroot}/usr/lib/dtrace/  # No systemtap support.
install -m 755 -d %{buildroot}/usr/lib/node_modules/
install -m 755 -d %{buildroot}%{_datadir}/%{name}

# Remove junk files from node_modules/ - we should probably take care of
# this in the installer.
for FILE in .gitmodules .gitignore .npmignore .travis.yml \*.py[co]; do
  find %{buildroot}/usr/lib/node_modules/ -name "$FILE" -delete
done


%files
/usr/bin/*
/usr/include/*
/usr/lib/node_modules/
/usr/share/doc/node/gdbinit
/usr/share/man/man1/node.1.gz
/usr/share/systemtap/tapset/node.stp
%{_datadir}/%{name}/
%doc CHANGELOG.md LICENSE README.md


%changelog
* Tue Jul 7 2015 Ali Ijaz Sheikh <ofrobots@google.com>
- Added gdbinit.

* Mon Apr 13 2015 Dan Varga <danvarga@gmail.com>
- Fix paths for changelog and manpage

* Thu Dec 4 2014 Ben Noordhuis <info@bnoordhuis.nl>
- Rename to iojs.

* Fri Jul 5 2013 Ben Noordhuis <info@bnoordhuis.nl>
- Initial release.
