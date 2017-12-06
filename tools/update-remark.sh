#!/usr/bin/env bash

# Shell script to update remark in the source tree to the latest release.

# Depends on npm and node being in $PATH.

# This script must be be in the tools directory when it runs because it uses
# $BASH_SOURCE[0] to determine directories to work in.

cd "$( dirname "${BASH_SOURCE[0]}" )"
rm -rf remark-cli
mkdir remark-cli-tmp
cd remark-cli-tmp
npm init --yes

npm install --global-style --no-bin-links --production --no-package-lock remark-cli@latest

cd node_modules/remark-cli
npm dedupe
cd ../..

# Install dmn if it is not in path.
type -P dmn || npm install -g dmn

# Use dmn to remove some unneeded files.
dmn -f clean

cd ..
mv remark-cli-tmp/node_modules/remark-cli remark-cli
rm -rf remark-cli-tmp/

rm -rf remark-preset-lint-node
mkdir remark-preset-lint-node-tmp
cd remark-preset-lint-node-tmp
npm init --yes

npm install --global-style --no-bin-links --production --no-package-lock remark-preset-lint-node@latest

cd node_modules/remark-preset-lint-node
npm dedupe
cd ../..

# Use dmn to remove some unneeded files.
dmn -f clean

cd ..
mv remark-preset-lint-node-tmp/node_modules/remark-preset-lint-node remark-preset-lint-node
rm -rf remark-preset-lint-node-tmp/
