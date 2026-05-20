#!/bin/bash
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

v8_root=$(readlink -f $(dirname $BASH_SOURCE)/../)
directories="src test/cctest test/unittests"

for directory in $directories; do
  headers=$(find "$v8_root/$directory" -name '*.h' -not -name '*-inl.h')
  for header in $headers; do
    inline_header_include=$(grep '#include ".*-inl.h"' "$header")
    if [ -n "$inline_header_include" ]; then
      echo "The following non-inline header seems to include an inline header:"
      echo "  Header : $header"
      echo "  Include: $inline_header_include"
      echo
    fi
  done
done

echo "Kthxbye."
