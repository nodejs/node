#!/bin/sh

# Shell script to update postject in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

cd "$( dirname "$0" )/.." || exit

(
    rm -rf postject-tmp
    mkdir postject-tmp
    cd postject-tmp || exit

    ROOT="$PWD/.."
    [ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
    [ -x "$NODE" ] || NODE=$(command -v node)
    NPM="$ROOT/deps/npm/bin/npm-cli.js"

    "$NODE" "$NPM" init --yes

    "$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts postject
)

rm -rf deps/postject
mkdir -p deps/postject/src
mv postject-tmp/node_modules/postject/* deps/postject/src
cp deps/postject/src/dist/api.js deps/postject/postject.js

rm -rf postject-tmp
