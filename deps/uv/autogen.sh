#!/bin/sh

# Copyright (c) 2013, Ben Noordhuis <info@bnoordhuis.nl>
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

set -eu
cd `dirname "$0"`

if [ "${1:-dev}" = "release" ]; then
    export LIBUV_RELEASE=true
else
    export LIBUV_RELEASE=false
fi

if [ "${LIBTOOLIZE:-}" = "" ] && [ "`uname`" = "Darwin" ]; then
  LIBTOOLIZE=glibtoolize
fi

ACLOCAL=${ACLOCAL:-aclocal}
AUTOCONF=${AUTOCONF:-autoconf}
AUTOMAKE=${AUTOMAKE:-automake}
LIBTOOLIZE=${LIBTOOLIZE:-libtoolize}

aclocal_version=`"$ACLOCAL" --version | head -n 1 | sed 's/[^.0-9]//g'`
autoconf_version=`"$AUTOCONF" --version | head -n 1 | sed 's/[^.0-9]//g'`
automake_version=`"$AUTOMAKE" --version | head -n 1 | sed 's/[^.0-9]//g'`
automake_version_major=`echo "$automake_version" | cut -d. -f1`
automake_version_minor=`echo "$automake_version" | cut -d. -f2`
libtoolize_version=`"$LIBTOOLIZE" --version | head -n 1 | sed 's/[^.0-9]//g'`

if [ $aclocal_version != $automake_version ]; then
    echo "FATAL: aclocal version appears not to be from the same as automake"
    exit 1
fi

UV_EXTRA_AUTOMAKE_FLAGS=
if test "$automake_version_major" -gt 1 || \
   test "$automake_version_major" -eq 1 && \
   test "$automake_version_minor" -gt 11; then
  # serial-tests is available in v1.12 and newer.
  UV_EXTRA_AUTOMAKE_FLAGS="$UV_EXTRA_AUTOMAKE_FLAGS serial-tests"
fi
echo "m4_define([UV_EXTRA_AUTOMAKE_FLAGS], [$UV_EXTRA_AUTOMAKE_FLAGS])" \
    > m4/libuv-extra-automake-flags.m4

(set -x
"$LIBTOOLIZE" --copy --force
"$ACLOCAL" -I m4
)
if $LIBUV_RELEASE; then
  "$AUTOCONF" -o /dev/null m4/libuv-check-versions.m4
  echo "
AC_PREREQ($autoconf_version)
AC_INIT([libuv-release-check], [0.0])
AM_INIT_AUTOMAKE([$automake_version])
LT_PREREQ($libtoolize_version)
AC_OUTPUT
" > m4/libuv-check-versions.m4
fi
(
set -x
"$AUTOCONF"
"$AUTOMAKE" --add-missing --copy
)
