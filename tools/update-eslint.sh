#!/bin/sh

# Shell script to update ESLint in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

cd "$( dirname "$0" )" || exit
rm -rf node_modules/eslint node_modules/eslint-plugin-markdown
(
    mkdir eslint-tmp
    cd eslint-tmp || exit

    ROOT="$PWD/../.."
    [ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
    [ -x "$NODE" ] || NODE=`command -v node`
    NPM="$ROOT/deps/npm"

    "$NODE" "$NPM" init --yes

    "$NODE" "$NPM" install --global-style --no-bin-links --production --no-package-lock eslint@latest
    "$NODE" "$NPM" install --global-style --no-bin-links --production --no-package-lock eslint-plugin-markdown@latest

    # Use dmn to remove some unneeded files.
    "$NODE" "$NPM" exec -- dmn@2.2.2 -f clean
    # Use removeNPMAbsolutePaths to remove unused data in package.json.
    # This avoids churn as absolute paths can change from one dev to another.
    "$NODE" "$NPM" exec -- removeNPMAbsolutePaths@1.0.4 .
)

mv eslint-tmp/node_modules/eslint node_modules/eslint
mv eslint-tmp/node_modules/eslint-plugin-markdown node_modules/eslint-plugin-markdown
rm -rf eslint-tmp/
