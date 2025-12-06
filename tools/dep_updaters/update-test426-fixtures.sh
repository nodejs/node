#!/bin/sh

set -e

REPO_URL="https://github.com/tc39/source-map-tests.git"
TARGET_DIR="$(dirname "$0")/../../test/fixtures/test426"
TMP_DIR="$(mktemp -d)"

git clone --depth=1 "$REPO_URL" "$TMP_DIR"

rsync -a --delete --exclude='.git' "$TMP_DIR"/ "$TARGET_DIR"/

rm -rf "$TMP_DIR"

echo "test426 fixtures updated from $REPO_URL."
