#!/bin/bash -e

# Handle arguments
while [ $# -gt 0 ]; do
  case "$1" in
    -p|--progress) PROGRESS="$2"; shift ;;
    -p=*) PROGRESS="${1#-p=}" ;;
    --progress=*) PROGRESS="${1#--progress=}" ;;
    --logfile) LOGFILE="$2"; shift ;;
    --logfile=*) LOGFILE="${1#--logfile=}" ;;
    *) echo "Unknown parameters $@" && exit 1;;
  esac
  shift
done

# Set default progress indicator to classic
PROGRESS=${PROGRESS:-classic}

# Always change the working directory to the project's root directory
cd $(dirname $0)/..

# Pass a $NODE environment variable from something like Makefile
# It should be a relative path to the node binary, e.g. ./node
if [ -z $NODE ]; then
  echo "No \$NODE executable provided, defaulting to out/Release/node." >&2
  NODE=out/Release/node
fi

# Ensure npm always uses the local node
export PATH="$PWD/`dirname $NODE`:$PATH"
unset NODE

rm -rf test-npm

# Make a copy of deps/npm to run the tests on
cp -r deps/npm test-npm

cd test-npm

# Do a rm first just in case deps/npm contained these
rm -rf npm-cache npm-tmp npm-prefix npm-userconfig npm-home
mkdir npm-cache npm-tmp npm-prefix npm-userconfig npm-home

# Set some npm env variables to point to our new temporary folders
export npm_config_cache="$(pwd)/npm-cache"
export npm_config_prefix="$(pwd)/npm-prefix"
export npm_config_tmp="$(pwd)/npm-tmp"
export npm_config_userconfig="$(pwd)/npm-userconfig"
export HOME="$(pwd)/npm-home"

# Make sure the binaries from the non-dev-deps are available
node cli.js rebuild
# Install npm devDependencies and run npm's tests
node cli.js install --ignore-scripts

# Run the tests with logging if set
if [ -n "$LOGFILE" ]; then
  echo "node cli.js run test-node -- --reporter=$PROGRESS | tee ../$LOGFILE"
  node cli.js run test-node -- --reporter=$PROGRESS | tee ../$LOGFILE
else
  echo "node cli.js run test-node -- --reporter=$PROGRESS"
  node cli.js run test-node -- --reporter=$PROGRESS
fi

# Move npm-debug.log up a directory if it exists
if [ -f npm-debug.log ]; then
  mv npm-debug.log ..
fi

# Clean up everything in one single shot
cd .. && rm -rf test-npm
