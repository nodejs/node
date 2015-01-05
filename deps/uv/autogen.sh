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

cd `dirname "$0"`

if [ "$LIBTOOLIZE" = "" ] && [ "`uname`" = "Darwin" ]; then
  LIBTOOLIZE=glibtoolize
fi

ACLOCAL=${ACLOCAL:-aclocal}
AUTOCONF=${AUTOCONF:-autoconf}
AUTOMAKE=${AUTOMAKE:-automake}
LIBTOOLIZE=${LIBTOOLIZE:-libtoolize}

automake_version=`"$AUTOMAKE" --version | head -n 1 | sed 's/[^.0-9]//g'`
automake_version_major=`echo "$automake_version" | cut -d. -f1`
automake_version_minor=`echo "$automake_version" | cut -d. -f2`

UV_EXTRA_AUTOMAKE_FLAGS=
if test "$automake_version_major" -gt 1 || \
   test "$automake_version_major" -eq 1 && \
   test "$automake_version_minor" -gt 11; then
  # serial-tests is available in v1.12 and newer.
  UV_EXTRA_AUTOMAKE_FLAGS="$UV_EXTRA_AUTOMAKE_FLAGS serial-tests"
fi
echo "m4_define([UV_EXTRA_AUTOMAKE_FLAGS], [$UV_EXTRA_AUTOMAKE_FLAGS])" \
    > m4/libuv-extra-automake-flags.m4

set -ex
"$LIBTOOLIZE"
"$ACLOCAL" -I m4
"$AUTOCONF"
"$AUTOMAKE" --add-missing --copy
