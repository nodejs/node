#!/bin/sh
set -e
# Shell script to update libuv in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
LIBUV_VERSION=$1

if [ "$#" -le 0 ]; then
  echo "Error: please provide an libuv version to update to"
  echo "	e.g. $0 1.44.2"
  exit 1
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
curl -sL "https://api.github.com/repos/libuv/libuv/tarball/v$LIBUV_VERSION" | tar xzf -
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
echo "$ git commit -m \"deps: update libuv to $LIBUV_VERSION\""
echo ""
