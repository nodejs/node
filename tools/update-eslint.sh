#!/bin/sh

# Shell script to update ESLint in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

cd "$( dirname "$0" )" || exit
rm -rf node_modules/eslint
(
    rm -rf eslint-tmp
    mkdir eslint-tmp
    cd eslint-tmp || exit

    ROOT="$PWD/../.."
    [ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
    [ -x "$NODE" ] || NODE=$(command -v node)
    NPM="$ROOT/deps/npm/bin/npm-cli.js"

    "$NODE" "$NPM" init --yes

    "$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts eslint
    # Uninstall plugins that we want to install so that they are removed from devDependencies.
    # Otherwise --production will cause them to be skipped.
    (cd node_modules/eslint && "$NODE" "$NPM" uninstall --ignore-scripts eslint-plugin-jsdoc eslint-plugin-markdown @babel/core @babel/eslint-parser @babel/plugin-syntax-import-assertions)
    (cd node_modules/eslint && "$NODE" "$NPM" install --no-save --no-bin-links --ignore-scripts --production --omit=peer eslint-plugin-jsdoc eslint-plugin-markdown @babel/core @babel/eslint-parser @babel/plugin-syntax-import-assertions)
    # Use dmn to remove some unneeded files.
    "$NODE" "$NPM" exec -- dmn@2.2.2 -f clean
    # TODO: Get this into dmn.
    find node_modules -name .package-lock.json -exec rm {} \;
    find node_modules -name 'README*' -exec rm {} \;
)

mv eslint-tmp/node_modules/eslint node_modules/eslint
rm -rf eslint-tmp/
