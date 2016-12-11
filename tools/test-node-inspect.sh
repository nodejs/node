#!/bin/bash

set -e

# always change the working directory to the project's root directory
cd $(dirname $0)/..

# pass a $NODE environment variable from something like Makefile
# it should point to either ./node or ./node.exe, depending on the platform
if [ -z $NODE ]; then
  echo "No node executable provided. Bailing." >&2
  exit 0
fi

rm -rf test-node-inspect

# make a copy of deps/node-inspect to run the tests on
cp -r deps/node-inspect test-node-inspect

cd test-node-inspect

# make sure our test does not leak into the general file system
mkdir npm-cache npm-tmp npm-prefix
export npm_config_cache="$(pwd)/npm-cache"
export npm_config_prefix="$(pwd)/npm-prefix"
export npm_config_tmp="$(pwd)/npm-tmp"

# ensure npm always uses the local node
export PATH="$(../$NODE -p 'require("path").resolve("..")'):$PATH"
unset NODE

node ../deps/npm/cli.js install

node ./node_modules/.bin/tap 'test/**/*.test.js'

# clean up everything one single shot
cd .. && rm -rf test-node-inspect
