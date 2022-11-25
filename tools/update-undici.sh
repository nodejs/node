#!/bin/sh

# Shell script to update undici in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

cd "$( dirname "$0" )/.." || exit
rm -rf deps/undici/src
rm -f deps/undici/undici.js

(
    rm -rf undici-tmp
    mkdir undici-tmp
    cd undici-tmp || exit

    ROOT="$PWD/.."
    [ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
    [ -x "$NODE" ] || NODE=$(command -v node)
    NPM="$ROOT/deps/npm/bin/npm-cli.js"

    "$NODE" "$NPM" init --yes

    "$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts undici
    cd node_modules/undici
    "$NODE" "$NPM" run build:node
    # get the new version of undici
    UNDICI_VERSION=$("$NODE" -p "require('./package.json').version")
    # update this version information in src/node_metadata.cc
    git show HEAD:src/node_metadata.cc | \
      sed "s/UNDICI_VERSION \"[0-9.]*\"/UNDICI_VERSION \"$UNDICI_VERSION\"/" \
      > "$ROOT/src/node_metadata.cc"
)

mv undici-tmp/node_modules/undici deps/undici/src
mv deps/undici/src/undici-fetch.js deps/undici/undici.js
cp deps/undici/src/LICENSE deps/undici/LICENSE

rm -rf undici-tmp/
