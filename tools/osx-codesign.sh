#!/bin/sh

set -x
set -e

# shellcheck disable=SC2154
[ -z "$SIGN" ] && \
  echo "No SIGN environment var.  Skipping codesign." >&2 && \
  exit 0

# All macOS executable binaries in the bundle must be codesigned with the
# hardened runtime enabled.
# See https://github.com/nodejs/node/pull/31459

# shellcheck disable=SC2154
codesign \
  --sign "$SIGN" \
  --entitlements tools/osx-entitlements.plist \
  --options runtime \
  --timestamp \
  "$PKGDIR"/bin/node
