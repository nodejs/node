#!/bin/sh

# Copyright (c) 2013, 2014, Ben Noordhuis <info@bnoordhuis.nl>
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

SED=sed
UNAME=`uname`

if [ "$UNAME" = Darwin ] || [ "$UNAME" = FreeBSD ]; then
  SED=gsed
fi

cd `dirname "$0"`/../

for FILE in src/*.cc; do
  $SED -rne 's/^using (\w+::\w+);$/\1/p' $FILE | sort -c || echo "in $FILE"
done

for FILE in src/*.cc; do
  for IMPORT in `$SED -rne 's/^using (\w+)::(\w+);$/\2/p' $FILE`; do
    if ! $SED -re '/^using (\w+)::(\w+);$/d' $FILE | grep -q "$IMPORT"; then
      echo "$IMPORT unused in $FILE"
    fi
  done
done
