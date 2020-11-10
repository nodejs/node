#!/bin/sh
set -e
# Shell script to update npm in the source tree to a specific version

BASE_DIR="$( pwd )"/
DEPS_DIR="$BASE_DIR"deps/
NPM_VERSION=$1

if [ "$#" -le 0 ]; then
  echo "Error: please provide an npm version to update to"
  exit 1
fi

WORKSPACE="$TMPDIR"update-npm-$NPM_VERSION/

if [ -d "$WORKSPACE" ]; then
  echo "Cleaning up old workspace"
  rm -rf "$WORKSPACE"
fi

echo "Making temporary workspace"

mkdir -p "$WORKSPACE"

cd "$WORKSPACE"

git clone git@github.com:npm/cli.git
cd cli

echo "Preparing npm release"

git checkout v"$NPM_VERSION"
make
make release

echo "Removing old npm"

cd "$DEPS_DIR"
rm -rf npm/

echo "Copying new npm"

tar zxf "$WORKSPACE"cli/release/npm-"$NPM_VERSION".tgz

echo "Deleting temporary workspace"

rm -rf "$WORKSPACE"

echo ""
echo "All done!"
echo ""
echo "Please git add npm, commit the new version, and whitespace-fix:"
echo ""
echo "$ git add -A deps/npm"
echo "$ git commit -m \"deps: upgrade npm to $NPM_VERSION\""
echo "$ git rebase --whitespace=fix master"
echo ""
