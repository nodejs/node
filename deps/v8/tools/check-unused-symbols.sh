#!/bin/bash
# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

v8_root=$(readlink -f $(dirname $BASH_SOURCE)/../)
symbols=$(
    grep \
        --only-matching \
        --perl-regexp 'V\(_, \K([^,\)]*)' \
        -- "$v8_root/src/heap-symbols.h")

# Find symbols which appear exactly once (in heap-symbols.h)
grep \
    --only-matching \
    --no-filename \
    --recursive \
    --fixed-strings "$symbols" \
    -- "$v8_root/src" "$v8_root/test/cctest" \
| sort \
| uniq -u \
| sed -e 's/.*/Heap symbol "&" seems to be unused./'

echo "Kthxbye."
