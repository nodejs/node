#!/bin/bash
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

v8_root=$(readlink -f $(dirname $BASH_SOURCE)/../)
bailouts=$(grep -oP 'V\(\K(k[^,]*)' "$v8_root/src/bailout-reason.h")

for bailout in $bailouts; do
  bailout_uses=$(grep -r $bailout "$v8_root/src" "$v8_root/test/cctest" | wc -l)
  if [ $bailout_uses -eq "1" ]; then
    echo "Bailout reason \"$bailout\" seems to be unused."
  fi
done

echo "Kthxbye."
