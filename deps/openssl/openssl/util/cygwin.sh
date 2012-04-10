#!/bin/bash
#
# This script configures, builds and packs the binary package for
# the Cygwin net distribution version of OpenSSL
#

# Uncomment when debugging
#set -x

CONFIG_OPTIONS="--prefix=/usr shared zlib no-idea no-rc5"
INSTALL_PREFIX=/tmp/install/INSTALL

VERSION=
SUBVERSION=$1

function cleanup()
{
  rm -rf ${INSTALL_PREFIX}/etc
  rm -rf ${INSTALL_PREFIX}/usr
}

function get_openssl_version()
{
  eval `grep '^VERSION=' Makefile`
  if [ -z "${VERSION}" ]
  then
    echo "Error: Couldn't retrieve OpenSSL version from Makefile."
    echo "       Check value of variable VERSION in Makefile."
    exit 1
  fi
}

function base_install()
{
  mkdir -p ${INSTALL_PREFIX}
  cleanup
  make install INSTALL_PREFIX="${INSTALL_PREFIX}"
}

function doc_install()
{
  DOC_DIR=${INSTALL_PREFIX}/usr/share/doc/openssl

  mkdir -p ${DOC_DIR}
  cp CHANGES CHANGES.SSLeay INSTALL LICENSE NEWS README ${DOC_DIR}

  create_cygwin_readme
}

function certs_install()
{
  CERTS_DIR=${INSTALL_PREFIX}/usr/ssl/certs

  mkdir -p ${CERTS_DIR}
  cp -rp certs/* ${CERTS_DIR}
}

function create_cygwin_readme()
{
  README_DIR=${INSTALL_PREFIX}/usr/share/doc/Cygwin
  README_FILE=${README_DIR}/openssl-${VERSION}.README

  mkdir -p ${README_DIR}
  cat > ${README_FILE} <<- EOF
	The Cygwin version has been built using the following configure:

	  ./config ${CONFIG_OPTIONS}

	The IDEA and RC5 algorithms are disabled due to patent and/or
	licensing issues.
	EOF
}

function create_profile_files()
{
  PROFILE_DIR=${INSTALL_PREFIX}/etc/profile.d

  mkdir -p $PROFILE_DIR
  cat > ${PROFILE_DIR}/openssl.sh <<- "EOF"
	export MANPATH="${MANPATH}:/usr/ssl/man"
	EOF
  cat > ${PROFILE_DIR}/openssl.csh <<- "EOF"
	if ( $?MANPATH ) then
	  setenv MANPATH "${MANPATH}:/usr/ssl/man"
	else
	  setenv MANPATH ":/usr/ssl/man"
	endif
	EOF
}

if [ -z "${SUBVERSION}" ]
then
  echo "Usage: $0 subversion"
  exit 1
fi

if [ ! -f config ]
then
  echo "You must start this script in the OpenSSL toplevel source dir."
  exit 1
fi

./config ${CONFIG_OPTIONS}

get_openssl_version

make depend || exit 1

make || exit 1

base_install

doc_install

certs_install

create_cygwin_readme

create_profile_files

cd ${INSTALL_PREFIX}
chmod u+w usr/lib/engines/*.so
strip usr/bin/*.exe usr/bin/*.dll usr/lib/engines/*.so
chmod u-w usr/lib/engines/*.so

# Runtime package
tar cjf libopenssl${VERSION//[!0-9]/}-${VERSION}-${SUBVERSION}.tar.bz2 \
     usr/bin/cyg*dll
# Base package
find etc usr/bin/openssl.exe usr/bin/c_rehash usr/lib/engines usr/share/doc \
     usr/ssl/certs usr/ssl/man/man[157] usr/ssl/misc usr/ssl/openssl.cnf \
     usr/ssl/private \
     -empty -o \! -type d |
tar cjfT openssl-${VERSION}-${SUBVERSION}.tar.bz2 -
# Development package
find usr/include usr/lib/*.a usr/lib/pkgconfig usr/ssl/man/man3 \
     -empty -o \! -type d |
tar cjfT openssl-devel-${VERSION}-${SUBVERSION}.tar.bz2 -

ls -l openssl-${VERSION}-${SUBVERSION}.tar.bz2
ls -l openssl-devel-${VERSION}-${SUBVERSION}.tar.bz2
ls -l libopenssl${VERSION//[!0-9]/}-${VERSION}-${SUBVERSION}.tar.bz2

cleanup

exit 0
