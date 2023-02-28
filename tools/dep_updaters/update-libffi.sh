#!/bin/sh
set -e
# Shell script to update libffi in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
LIBFFI_VERSION=$1

if [ "$#" -le 0 ]; then
  echo "Error: please provide a libffi version to update to"
  echo "	e.g. $0 1.0.0"
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

echo "Fetching libffi source archive..."
curl -sL "https://api.github.com/repos/libffi/libffi/tarball/v$LIBFFI_VERSION" | tar xzf -
mv libffi-* libffi

echo "Replacing existing libffi (except GYP build files)"
mv "$DEPS_DIR/libffi/"*.gyp "$WORKSPACE/libffi/"
rm -rf "$DEPS_DIR/libffi"
mv "$WORKSPACE/libffi" "$DEPS_DIR/"

echo "All done!"
echo ""
echo "Please git add libffi, commit the new version:"
echo ""
echo "$ git add -A deps/libffi"
echo "$ git commit -m \"deps: update libffi to $LIBFFI_VERSION\""
echo ""
