#!/bin/sh

set -ex

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
if ! [ -x "$NODE" ]; then
    echo 'node not found' >&2
    exit 1
fi
NPM="$BASE_DIR/deps/npm/bin/npm-cli.js"

latest_commit=$("$NODE" "$BASE_DIR/tools/doc/get-latest-commit.mjs")

rm -rf node_modules/ package-lock.json

"$NODE" "$NPM" install "https://github.com/nodejs/doc-kit/archive/$latest_commit.tar.gz" --omit=dev
