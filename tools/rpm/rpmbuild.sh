#!/bin/sh

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

set -e

TOOLSDIR=`dirname $0`
TOPLEVELDIR="$TOOLSDIR/../.."

RPMBUILD_PATH="${RPMBUILD_PATH:-$HOME/rpmbuild}"
if [ ! -d "$RPMBUILD_PATH" ]; then
  echo "Run rpmdev-setuptree first."
  exit 1
fi

if [ $# -ge 1 ]; then
  VERSION=$1
else
  FILE="$TOPLEVELDIR/src/node_version.h"
  MAJOR=`sed -nre 's/#define NODE_MAJOR_VERSION ([0-9]+)/\1/p' "$FILE"`
  MINOR=`sed -nre 's/#define NODE_MINOR_VERSION ([0-9]+)/\1/p' "$FILE"`
  PATCH=`sed -nre 's/#define NODE_PATCH_VERSION ([0-9]+)/\1/p' "$FILE"`
  VERSION="$MAJOR.$MINOR.$PATCH"
fi

set -x

sed -re "s/%define _version .+/%define _version ${VERSION}/" \
    "$TOOLSDIR/node.spec" > $RPMBUILD_PATH/SPECS/node.spec
tar --exclude-vcs --transform="s|^|node-${VERSION}/|" \
    -czf $RPMBUILD_PATH/SOURCES/node-v${VERSION}.tar.gz .
rpmbuild $* -ba $RPMBUILD_PATH/SPECS/node.spec
