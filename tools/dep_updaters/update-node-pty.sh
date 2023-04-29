#!/bin/sh

# Shell script to update node-pty in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

ROOT=$(cd "$(dirname "$0")/../.." && pwd)

[ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM="$ROOT/deps/npm/bin/npm-cli.js"

NEW_VERSION=$("$NODE" "$NPM" view node-pty latest)
CURRENT_VERSION=$("$NODE" -p "require('./tools/node_modules/node-pty/package.json').version")

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because node-pty is on the latest version."
  exit 0
fi

cd "$( dirname "$0" )" || exit
rm -rf ../node_modules/node-pty
(
    rm -rf node-pty-tmp
    mkdir node-pty-tmp
    cd node-pty-tmp || exit

    "$NODE" "$NPM" init --yes
    "$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts node-pty
)

mv node-pty-tmp/node_modules/node-pty ../node_modules/node-pty
rm -rf node-pty-tmp/

echo ".buildstamp
build/" >> ../node_modules/node-pty/.gitignore

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
