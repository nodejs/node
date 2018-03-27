#!/usr/bin/env bash

# Shell script to update ESLint in the source tree to the latest release.

# Depends on npm and node being in $PATH.

# This script must be be in the tools directory when it runs because it uses
# $BASH_SOURCE[0] to determine directories to work in.

cd "$( dirname "${BASH_SOURCE[0]}" )/../deps/"
rm -rf node-report
mkdir node-report-tmp
cd node-report-tmp
npm init --yes

LATEST=`npm view node-report version`
wget "https://github.com/nodejs/node-report/archive/v${LATEST}.tar.gz"
tar xfz "v${LATEST}.tar.gz"

cd "node-report-${LATEST}"
npm install --global-style --no-bin-links --production --no-package-lock --ignore-scripts

# Install dmn if it is not in path.
type -P dmn || npm install -g dmn

# Use dmn to remove some unneeded files.
dmn -f clean

cd ../..
mv "node-report-tmp/node-report-${LATEST}" node-report
rm -rf node-report-tmp/
