#!/bin/sh

set -e

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)

TARGET_DIR="$BASE_DIR/test/fixtures/test426"
README="$BASE_DIR/test/test426/README.md"
TARBALL_URL=$(curl -fsIo /dev/null -w '%header{Location}' https://github.com/tc39/source-map-tests/archive/HEAD.tar.gz)
SHA=$(basename "$TARBALL_URL")

TMP_DIR="$(mktemp -d)"

rm -rf "$TARGET_DIR"
mkdir "$TARGET_DIR"
curl -f "$TARBALL_URL" | tar -xz --strip-components 1 -C "$TARGET_DIR"

rm -rf "$TMP_DIR"

TMP_FILE=$(mktemp)
sed "s#https://github.com/tc39/source-map-tests/commit/[0-9a-f]*#https://github.com/tc39/source-map-tests/commit/$SHA#" "$README" > "$TMP_FILE"
mv "$TMP_FILE" "$README"

echo "test426 fixtures updated to $SHA."
