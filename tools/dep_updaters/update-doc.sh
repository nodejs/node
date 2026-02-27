#!/bin/sh

set -ex

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
DEPS_DIR="$BASE_DIR/deps"
NPM="$DEPS_DIR/npm/bin/npm-cli.js"

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

cd "$BASE_DIR/tools/doc"

OLD_VERSION=$(jq '.dependencies["@nodejs/doc-kit"]' package.json)
LATEST_COMMIT=$("$NODE" get-latest-commit.mjs)
NEW_VERSION="https://github.com/nodejs/doc-kit/archive/$LATEST_COMMIT.tar.gz"

compare_dependency_version "doc-kit" "$OLD_VERSION" "$NEW_VERSION"

rm -rf node_modules/ package-lock.json

"$NODE" "$NPM" install "$NEW_VERSION" --omit=dev

finalize_version_update "doc-kit" "$NEW_VERSION"
