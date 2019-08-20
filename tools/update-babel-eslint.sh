#!/usr/bin/env bash

# Shell script to update babel-eslint in the source tree to the latest release.

# Depends on npm, npx, and node being in $PATH.

# This script must be be in the tools directory when it runs because it uses
# $BASH_SOURCE[0] to determine directories to work in.

cd "$( dirname "${BASH_SOURCE[0]}" )"
rm -rf node_modules/babel-eslint
mkdir babel-eslint-tmp
cd babel-eslint-tmp
npm init --yes

npm install --global-style --no-bin-links --production --no-package-lock babel-eslint@latest

# Use dmn to remove some unneeded files.
npx dmn@2.2.2 -f clean
# Use removeNPMAbsolutePaths to remove unused data in package.json.
# This avoids churn as absolute paths can change from one dev to another.
npx removeNPMAbsolutePaths@1.0.4 .

cd ..
mv babel-eslint-tmp/node_modules/babel-eslint node_modules/babel-eslint
rm -rf babel-eslint-tmp/
