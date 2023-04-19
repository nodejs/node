#!/bin/sh
set -e
# Shell script to update zlib in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

CURRENT_VERSION=$(grep "#define ZLIB_VERSION" "$DEPS_DIR/zlib/zlib.h" | sed -n "s/^.*VERSION \"\(.*\)\"/\1/p")
 
NEW_VERSION_ZLIB_H=$(curl -s "https://chromium.googlesource.com/chromium/src/+/refs/heads/main/third_party/zlib/zlib.h?format=TEXT" | base64 --decode)

NEW_VERSION=$(printf '%s' "$NEW_VERSION_ZLIB_H" | grep "#define ZLIB_VERSION" | sed -n "s/^.*VERSION \"\(.*\)\"/\1/p")

echo "Comparing $NEW_VERSION with $CURRENT_VERSION"

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because zlib is on the latest version."
  exit 0
fi

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cd "$WORKSPACE"

mkdir zlib

ZLIB_TARBALL=zlib.tar.gz

echo "Fetching zlib source archive"
curl -sL -o $ZLIB_TARBALL https://chromium.googlesource.com/chromium/src/+archive/refs/heads/main/third_party/$ZLIB_TARBALL

gzip -dc "$ZLIB_TARBALL" | tar xf - -C zlib/

rm "$ZLIB_TARBALL"

cp "$DEPS_DIR/zlib/zlib.gyp" "$DEPS_DIR/zlib/GN-scraper.py" "$DEPS_DIR/zlib/win32/zlib.def" "$DEPS_DIR"

rm -rf "$DEPS_DIR/zlib" zlib/.git

mv zlib "$DEPS_DIR/"

mv "$DEPS_DIR/zlib.gyp" "$DEPS_DIR/GN-scraper.py" "$DEPS_DIR/zlib/"

mkdir "$DEPS_DIR/zlib/win32"

mv "$DEPS_DIR/zlib.def" "$DEPS_DIR/zlib/win32"

perl -i -pe 's|^#include "chromeconf.h"|//#include "chromeconf.h"|' deps/zlib/zconf.h

echo "All done!"
echo ""
echo "Make sure to update the deps/zlib/zlib.gyp if any significant changes have occurred upstream"
echo ""
echo "Please git add zlib, commit the new version:"
echo ""
echo "$ git add -A deps/zlib"
echo "$ git commit -m \"deps: update zlib to $NEW_VERSION\""
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
