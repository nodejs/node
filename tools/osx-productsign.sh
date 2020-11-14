#!/bin/sh

set -x
set -e

# shellcheck disable=SC2154
[ -z "$SIGN" ] && \
  echo "No SIGN environment var.  Skipping codesign." >&2 && \
  exit 0

# shellcheck disable=SC2154
productsign --sign "$SIGN" "$PKG" "$PKG"-SIGNED
# shellcheck disable=SC2154
mv "$PKG"-SIGNED "$PKG"
