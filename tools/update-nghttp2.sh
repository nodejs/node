#!/bin/sh
set -e
# Shell script to update nghttp2 in the source treee to specific version

BASE_DIR="$( pwd )"/
DEPS_DIR="$BASE_DIR"deps/
NGHTTP2_VERSION=$1

if [ "$#" -le 0 ]; then
  echo "Error: please provide an nghttp2 version to update to"
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

NGHTTP2_REF="v$NGHTTP2_VERSION"
NGHTTP2_TARBALL="nghttp2-$NGHTTP2_VERSION.tar.gz"

cd "$WORKSPACE"

echo "Fetching nghttp2 source archive"
curl -sL -o "$NGHTTP2_TARBALL" "https://github.com/nghttp2/nghttp2/releases/download/$NGHTTP2_REF/$NGHTTP2_TARBALL"
gzip -dc "$NGHTTP2_TARBALL" | tar xf -
rm "$NGHTTP2_TARBALL"
mv "nghttp2-$NGHTTP2_VERSION" nghttp2

echo "Removing everything, except lib/ and COPYING"
cd nghttp2
for dir in *; do
  if [ "$dir" = "lib" ] || [ "$dir" = "COPYING" ]; then
    continue
  fi
  rm -rf "$dir"
done

echo "Copying existing gyp files"
cp "$DEPS_DIR/nghttp2/nghttp2.gyp" "$WORKSPACE/nghttp2"

echo "Replacing existing nghttp2"
rm -rf "$DEPS_DIR/nghttp2"
mv "$WORKSPACE/nghttp2" "$DEPS_DIR/"

echo "All done!"
echo ""
echo "Please git add nghttp2, commit the new version:"
echo ""
echo "$ git add -A deps/nghttp2"
echo "$ git commit -m \"deps: update nghttp2 to $NGHTTP2_VERSION\""
echo ""
