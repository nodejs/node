#!/bin/bash

set -x
set -e

if [ "X$SIGN" == "X" ]; then
  echo "No SIGN environment var.  Skipping codesign." >&2
  exit 0
fi

# All macOS executable binaries in the bundle must be codesigned with the
# hardened runtime enabled.
# See https://github.com/nodejs/node/pull/31459

codesign \
  --sign "$SIGN" \
  --entitlements tools/osx-entitlements.plist \
  --options runtime \
  --timestamp \
  "$PKGDIR"/bin/node
