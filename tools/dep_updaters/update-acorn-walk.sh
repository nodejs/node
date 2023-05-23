#!/bin/sh

# Shell script to update acorn-walk in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
[ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM="$ROOT/deps/npm/bin/npm-cli.js"

NEW_VERSION=$("$NODE" "$NPM" view acorn-walk dist-tags.latest)
CURRENT_VERSION=$("$NODE" -p "require('./deps/acorn/acorn-walk/package.json').version")

echo "Comparing $NEW_VERSION with $CURRENT_VERSION"

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because Acorn-walk is on the latest version."
  exit 0
fi

cd "$( dirname "$0" )/../.." || exit

rm -rf deps/acorn/acorn-walk

(
    rm -rf acorn-walk-tmp
    mkdir acorn-walk-tmp
    cd acorn-walk-tmp || exit

    "$NODE" "$NPM" init --yes

    "$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts "acorn-walk@$NEW_VERSION"
)

mv acorn-walk-tmp/node_modules/acorn-walk deps/acorn

rm -rf acorn-walk-tmp/

echo "All done!"
echo ""
echo "Please git add acorn-walk, commit the new version:"
echo ""
echo "$ git add -A deps/acorn-walk"
echo "$ git commit -m \"deps: update acorn-walk to $NEW_VERSION\""
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
