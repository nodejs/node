#!/bin/bash

set -x
set -e

if [ "X$SIGN" == "X" ]; then
  echo "No SIGN environment var.  Skipping codesign." >&2
  exit 0
fi

codesign -s "$SIGN" "$PKGDIR"/usr/local/bin/node
