#!/usr/bin/env bash

# Shell script to update rimraf in the source tree to the latest release.

# Depends on npm, npx, and node being in $PATH.

# This script must be be in the tools directory when it runs because it uses
# $BASH_SOURCE[0] to determine directories to work in.

cd "$( dirname "${BASH_SOURCE[0]}" )"
rm -rf ../deps/rimraf
mkdir rimraf-tmp
cd rimraf-tmp
npm init --yes

npm install --global-style --no-bin-links --production --no-package-lock rimraf@latest
cd node_modules/rimraf

# Use dmn to remove some unneeded files.
npx dmn@2.2.2 -f clean

# Use removeNPMAbsolutePaths to remove unused data in package.json.
# This avoids churn as absolute paths can change from one dev to another.
npx removeNPMAbsolutePaths@1.0.4 .

# Remove rimraf's node_modules as we don't use glob, as well as it's binary
rm -rf ./node_modules
rm bin.js

cd ../../..
mv rimraf-tmp/node_modules/rimraf ../deps/rimraf
rm -rf rimraf-tmp/
