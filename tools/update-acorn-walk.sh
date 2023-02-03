#!/bin/sh

# Shell script to update acorn-walk in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

cd "$( dirname "$0" )/.." || exit
rm -rf deps/acorn/acorn-walk

(
    rm -rf acorn-walk-tmp
    mkdir acorn-walk-tmp
    cd acorn-walk-tmp || exit

    ROOT="$PWD/.."
    [ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
    [ -x "$NODE" ] || NODE=$(command -v node)
    NPM="$ROOT/deps/npm/bin/npm-cli.js"

    "$NODE" "$NPM" init --yes

    "$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts acorn-walk
)

mv acorn-walk-tmp/node_modules/acorn-walk deps/acorn

rm -rf acorn-walk-tmp/
