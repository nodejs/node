#!/usr/bin/env bash

# Shell script to update babel-eslint in the source tree to the latest release.

# Depends on npm and node being in $PATH.

# This script must be be in the tools directory when it runs because it uses
# $BASH_SOURCE[0] to determine directories to work in.

cd "$( dirname "${BASH_SOURCE[0]}" )"
rm -rf node_modules/babel-eslint
mkdir babel-eslint-tmp
cd babel-eslint-tmp
npm init --yes

npm install --global-style --no-bin-links --production --no-package-lock babel-eslint@latest

# Install dmn if it is not in path.
type -P dmn || npm install -g dmn

# Use dmn to remove some unneeded files.
dmn -f clean

cd ..
mv babel-eslint-tmp/node_modules/babel-eslint node_modules/babel-eslint
rm -rf babel-eslint-tmp/
