#!/bin/sh
set -e
# Shell script to update c-ares in the source tree to a specific version

BASE_DIR="$( pwd )"/
DEPS_DIR="$BASE_DIR"deps/
ARES_VERSION=$1

if [ "$#" -le 0 ]; then
  echo "Error: please provide an c-ares version to update to"
  echo "	e.g. $0 1.18.1"
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

ARES_REF="cares-$(echo "$ARES_VERSION" | tr . _)"
ARES_TARBALL="c-ares-$ARES_VERSION.tar.gz"

cd "$WORKSPACE"

echo "Fetching c-ares source archive"
curl -sL -o "$ARES_TARBALL" "https://github.com/c-ares/c-ares/releases/download/$ARES_REF/$ARES_TARBALL"
gzip -dc "$ARES_TARBALL" | tar xf -
rm "$ARES_TARBALL"
mv "c-ares-$ARES_VERSION" cares

echo "Removing tests"
rm -rf "$WORKSPACE/cares/test"

echo "Copying existing .gitignore, config and gyp files"
cp -R "$DEPS_DIR/cares/config" "$WORKSPACE/cares"
cp "$DEPS_DIR/cares/.gitignore" "$WORKSPACE/cares"
cp "$DEPS_DIR/cares/cares.gyp" "$WORKSPACE/cares"

echo "Replacing existing c-ares"
rm -rf "$DEPS_DIR/cares"
mv "$WORKSPACE/cares" "$DEPS_DIR/"

echo "All done!"
echo ""
echo "Please git add c-ares, commit the new version:"
echo ""
echo "$ git add -A deps/cares"
echo "$ git commit -m \"deps: update c-ares to $ARES_VERSION\""
echo ""
