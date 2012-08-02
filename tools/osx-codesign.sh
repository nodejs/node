#!/bin/bash

set -x
set -e

if ! [ -n "$SIGN" ] && [ $STEP -eq 1 ]; then
  echo "No SIGN environment var.  Skipping codesign." >&2
  exit 0
fi

codesign -s "$SIGN" "$PKGDIR"/usr/local/bin/node
codesign -s "$SIGN" "$PKGDIR"/32/usr/local/bin/node
