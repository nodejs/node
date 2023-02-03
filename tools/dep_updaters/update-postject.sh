#!/bin/sh

# Shell script to update postject in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

NEW_VERSION=$(npm view postject dist-tags.latest)
CURRENT_VERSION=$(node -p "require('./test/fixtures/postject-copy/node_modules/postject/package.json').version")

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because Postject is on the latest version."
  exit 0
fi

echo "NEW_VERSION=$NEW_VERSION" >> $GITHUB_ENV
cd "$( dirname "$0" )/../.." || exit
rm -rf test/fixtures/postject-copy
mkdir test/fixtures/postject-copy
cd test/fixtures/postject-copy || exit

ROOT="$PWD/../../.."
[ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM="$ROOT/deps/npm/bin/npm-cli.js"

"$NODE" "$NPM" init --yes

"$NODE" "$NPM" install --no-bin-links --ignore-scripts postject

# TODO(RaisinTen): Replace following with $WORKSPACE
cd ../../..
rm -rf deps/postject
mkdir deps/postject
cp test/fixtures/postject-copy/node_modules/postject/LICENSE deps/postject
cp test/fixtures/postject-copy/node_modules/postject/dist/postject-api.h deps/postject
