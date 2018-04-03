#!/usr/bin/env bash

# Shell script to update ESLint in the source tree to the latest release.

# Depends on npm and node being in $PATH.

# This script must be be in the tools directory when it runs because it uses
# $BASH_SOURCE[0] to determine directories to work in.

cd "$( dirname "${BASH_SOURCE[0]}" )"
rm -rf node_modules/eslint
mkdir eslint-tmp
cd eslint-tmp
npm init --yes

npm install --global-style --no-bin-links --production --no-package-lock eslint@latest
cd node_modules/eslint

npm install --no-bin-links --production --no-package-lock eslint-plugin-markdown@next
cd ../..

# Install dmn if it is not in path.
type -P dmn || npm install -g dmn

# Use dmn to remove some unneeded files.
dmn -f clean

cd ..
mv eslint-tmp/node_modules/eslint node_modules/eslint
rm -rf eslint-tmp/
