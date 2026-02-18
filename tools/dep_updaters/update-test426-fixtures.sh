#!/bin/sh

set -ex

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)

TARGET_DIR="$BASE_DIR/test/fixtures/test426"
README="$BASE_DIR/test/test426/README.md"
TARBALL_URL=$(curl -fsIo /dev/null -w '%header{Location}' https://github.com/tc39/source-map-tests/archive/HEAD.tar.gz)
SHA=$(basename "$TARBALL_URL")

CURRENT_SHA=$(sed -n 's#^.*https://github.com/tc39/source-map-tests/commit/\([0-9a-f]*\).*$#\1#p' "$README")

if [ "$CURRENT_SHA" = "$SHA" ]; then
  echo "Already up-to-date"
  exit 0
fi

rm -rf "$TARGET_DIR"
mkdir "$TARGET_DIR"
curl -f "$TARBALL_URL" | tar -xz --strip-components 1 -C "$TARGET_DIR"

TMP_FILE=$(mktemp)
sed "s/$CURRENT_SHA/$SHA/" "$README" > "$TMP_FILE"
mv "$TMP_FILE" "$README"

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$SHA"
