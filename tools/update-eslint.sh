#!/usr/bin/env bash

# Shell script to update ESLint in the source tree to the latest release.

# Depends on npm and node being in $PATH.

# This script must be be in the tools directory when it runs because it uses
# $BASH_SOURCE[0] to determine directories to work in.

cd "$( dirname "${BASH_SOURCE[0]}" )"
rm -rf eslint
mkdir eslint-tmp
cd eslint-tmp

npm install --global-style --no-bin-links --production eslint@latest
cd node_modules/eslint

npm install --no-bin-links --production eslint-plugin-markdown@next
cd ../..

# Install dmn if it is not in path.
type -P dmn || npm install -g dmn

# Use dmn to remove some unneeded files.
dmn -f clean

cd ..
mv eslint-tmp/node_modules/eslint eslint
rm -rf eslint-tmp/
