#!/bin/bash

# Copyright 2025 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

echo 'Finding inl headers with corresponding non-inl header, and making'
echo 'sure that the inl header includes the non-inl header first.'

while read inlh; do
  h=${inlh%-inl.h}.h
  [[ -f $h ]] || continue
  echo -n .
  awk '
BEGIN {
  i=0
}

/^#include / {
  if (i==0) {
    print("#include \"'$h'\"");
    print("// Include the non-inl header before the rest of the headers.");
    print("");
  }
  i=1
}

{
  if ($0 != "#include \"'$h'\"" &&
      $0 != "// Include the non-inl header before the rest of the headers.") {
    print $0;
  }
}
  ' $inlh >$inlh.new && mv $inlh.new $inlh
done < <(
  # Exclude macro-assembler-<ARCH>-inl.h headers because they have special
  # include rules (arch-specific macro assembler headers must be included
  # via the general macro-assembler.h).
  find src -name '*-inl.h' -and -not -name 'macro-assembler-*-inl.h'
)
echo

echo 'Done. Make sure to run "git cl format" and check "git diff".'
