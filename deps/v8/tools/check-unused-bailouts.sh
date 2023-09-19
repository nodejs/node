#!/bin/bash
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

v8_root=$(readlink -f $(dirname $BASH_SOURCE)/../)
bailouts=$(
    grep \
        --only-matching \
        --perl-regexp 'V\(\K(k[^,]*)' \
        -- "$v8_root/src/bailout-reason.h")

# Find bailouts which appear exactly once (in bailout-reason.h)
grep \
    --only-matching \
    --no-filename \
    --recursive \
    --word-regexp \
    --fixed-strings "$bailouts" \
    -- "$v8_root/src" "$v8_root/test/cctest" \
| sort \
| uniq -u \
| sed -e 's/.*/Bailout reason "&" seems to be unused./'

echo "Kthxbye."
