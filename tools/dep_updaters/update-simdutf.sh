#!/bin/sh
set -e
# Shell script to update simdutf in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
SIMDUTF_VERSION=$1

if [ "$#" -le 0 ]; then
  echo "Error: please provide an simdutf version to update to"
  echo "	e.g. $0 2.0.3"
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

SIMDUTF_REF="v$SIMDUTF_VERSION"
SIMDUTF_ZIP="simdutf-$SIMDUTF_VERSION.zip"
SIMDUTF_LICENSE="LICENSE-MIT"

cd "$WORKSPACE"

echo "Fetching simdutf source archive..."
curl -sL -o "$SIMDUTF_ZIP" "https://github.com/simdutf/simdutf/releases/download/$SIMDUTF_REF/singleheader.zip"
unzip "$SIMDUTF_ZIP"
rm "$SIMDUTF_ZIP"
rm ./*_demo.cpp

curl -sL -o "$SIMDUTF_LICENSE" "https://raw.githubusercontent.com/simdutf/simdutf/HEAD/LICENSE-MIT"

echo "Replacing existing simdutf (except GYP build files)"
mv "$DEPS_DIR/simdutf/"*.gyp "$DEPS_DIR/simdutf/README.md" "$WORKSPACE/"
rm -rf "$DEPS_DIR/simdutf"
mv "$WORKSPACE" "$DEPS_DIR/simdutf"

echo "All done!"
echo ""
echo "Please git add simdutf, commit the new version:"
echo ""
echo "$ git add -A deps/simdutf"
echo "$ git commit -m \"deps: update simdutf to $SIMDUTF_VERSION\""
echo ""
