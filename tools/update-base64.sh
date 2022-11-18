#!/bin/sh
set -e
# Shell script to update base64 in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
BASE64_VERSION=$1

if [ "$#" -le 0 ]; then
  echo "Error: please provide an base64 version to update to"
  echo "	e.g. $0 0.4.0"
  exit 1
fi

echo "Making temporary workspace"

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

cd "$WORKSPACE"

echo "Fetching base64 source archive"
curl -sL "https://api.github.com/repos/aklomp/base64/tarball/v$BASE64_VERSION" | tar xzf -
mv aklomp-base64-* base64

echo "Replacing existing base64"
rm -rf "$DEPS_DIR/base64/base64"
mv "$WORKSPACE/base64" "$DEPS_DIR/base64/"

# Build configuration is handled by `deps/base64/base64.gyp`, but since `config.h` has to be present for the build
# to work, we create it and leave it empty.
echo "// Intentionally empty" >> "$DEPS_DIR/base64/base64/lib/config.h"

echo "All done!"
echo ""
echo "Please git add base64/base64, commit the new version:"
echo ""
echo "$ git add -A deps/base64/base64"
echo "$ git commit -m \"deps: update base64 to $BASE64_VERSION\""
echo ""
