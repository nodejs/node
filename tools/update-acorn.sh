#!/bin/sh

# Shell script to update acorn in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

cd "$( dirname "$0" )/.." || exit
rm -rf deps/acorn/acorn

(
    rm -rf acorn-tmp
    mkdir acorn-tmp
    cd acorn-tmp || exit

    ROOT="$PWD/.."
    [ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
    [ -x "$NODE" ] || NODE=$(command -v node)
    NPM="$ROOT/deps/npm/bin/npm-cli.js"

    "$NODE" "$NPM" init --yes

    "$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts acorn
    cd node_modules/acorn
    # get acorn version
    ACORN_VERSION=$("$NODE" -p "require('./package.json').version")
    # update this version information in src/acorn_version.h
    FILE_PATH="$ROOT/src/acorn_version.h"
    echo "// This is an auto generated file, please do not edit." > "$FILE_PATH"
    echo "// Refer to tools/update-acorn.sh" >> "$FILE_PATH"
    echo "#ifndef SRC_ACORN_VERSION_H_" >> "$FILE_PATH"
    echo "#define SRC_ACORN_VERSION_H_" >> "$FILE_PATH"
    echo "#define ACORN_VERSION \"$ACORN_VERSION\"" >> "$FILE_PATH"
    echo "#endif  // SRC_ACORN_VERSION_H_" >> "$FILE_PATH"
)

mv acorn-tmp/node_modules/acorn deps/acorn

rm -rf acorn-tmp/
