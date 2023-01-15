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
    # update this version information in src/undici_version.h
    FILE_PATH="$ROOT/src/undici_version.h"
    echo "// This is an auto generated file, please do not edit." > "$FILE_PATH"
    echo "// Refer to tools/update-undici.sh" >> "$FILE_PATH"
    echo "#ifndef SRC_UNDICI_VERSION_H_" >> "$FILE_PATH"
    echo "#define SRC_UNDICI_VERSION_H_" >> "$FILE_PATH"
    echo "#define UNDICI_VERSION \"$UNDICI_VERSION\"" >> "$FILE_PATH"
    echo "#endif  // SRC_UNDICI_VERSION_H_" >> "$FILE_PATH"
)

mv undici-tmp/node_modules/undici deps/undici/src
mv deps/undici/src/undici-fetch.js deps/undici/undici.js
cp deps/undici/src/LICENSE deps/undici/LICENSE

rm -rf undici-tmp/
