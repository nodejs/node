#!/bin/sh
set -e
# Shell script to update libuv in the source tree to a specific version

if [ "$#" -le 0 ]; then
  echo "Error: please provide an libuv version to update to"
  echo "	e.g. $0 1.44.2"
  exit 1
fi

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

NEW_VERSION=$1
VERSION_H="$DEPS_DIR/uv/include/uv/version.h"
CURRENT_MAJOR_VERSION=$(grep "#define UV_VERSION_MAJOR" "$VERSION_H" | sed -n "s/^.*MAJOR \(.*\)/\1/p")
CURRENT_MINOR_VERSION=$(grep "#define UV_VERSION_MINOR" "$VERSION_H" | sed -n "s/^.*MINOR \(.*\)/\1/p")
CURRENT_PATCH_VERSION=$(grep "#define UV_VERSION_PATCH" "$VERSION_H" | sed -n "s/^.*PATCH \(.*\)/\1/p")
CURRENT_IS_RELEASE=$(grep "#define UV_VERSION_IS_RELEASE" "$VERSION_H" | sed -n "s/^.*RELEASE \(.*\)/\1/p")
CURRENT_SUFFIX_VERSION=$(grep "#define UV_VERSION_SUFFIX" "$VERSION_H" | sed -n "s/^.*SUFFIX \"\(.*\)\"/\1/p")
SUFFIX_STRING=$([ "$CURRENT_IS_RELEASE" = 1 ] || [ -z "$CURRENT_SUFFIX_VERSION" ] && echo "" || echo "-$CURRENT_SUFFIX_VERSION")
CURRENT_VERSION="$CURRENT_MAJOR_VERSION.$CURRENT_MINOR_VERSION.$CURRENT_PATCH_VERSION$SUFFIX_STRING"

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because libuv is on the latest version."
  exit 0
fi

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

cd "$WORKSPACE"

echo "Fetching libuv source archive..."
curl -sL "https://api.github.com/repos/libuv/libuv/tarball/v$NEW_VERSION" | tar xzf -
mv libuv-libuv-* uv

echo "Replacing existing libuv (except GYP build files)"
mv "$DEPS_DIR/uv/"*.gyp "$DEPS_DIR/uv/"*.gypi "$WORKSPACE/uv/"
rm -rf "$DEPS_DIR/uv"
mv "$WORKSPACE/uv" "$DEPS_DIR/"

echo "All done!"
echo ""
echo "Please git add uv, commit the new version:"
echo ""
echo "$ git add -A deps/uv"
echo "$ git commit -m \"deps: update libuv to $NEW_VERSION\""
echo ""
