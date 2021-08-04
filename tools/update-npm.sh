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

echo "Making temporary workspace"

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

cd "$WORKSPACE"

git clone --depth=1 --branch="v$NPM_VERSION" git@github.com:npm/cli.git
cd cli

echo "Preparing npm release"

make
make release

echo "Removing old npm"

cd "$DEPS_DIR"
rm -rf npm/

echo "Copying new npm"

tar zxf "$WORKSPACE"/cli/release/npm-"$NPM_VERSION".tgz

echo ""
echo "All done!"
echo ""
echo "Please git add npm, commit the new version, and whitespace-fix:"
echo ""
echo "$ git add -A deps/npm"
echo "$ git commit -m \"deps: upgrade npm to $NPM_VERSION\""
echo "$ git rebase --whitespace=fix master"
echo ""
