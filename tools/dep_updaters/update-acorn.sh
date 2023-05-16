#!/bin/sh

# Shell script to update acorn in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
[ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM="$ROOT/deps/npm/bin/npm-cli.js"

NEW_VERSION=$("$NODE" "$NPM" view acorn dist-tags.latest)
CURRENT_VERSION=$("$NODE" -p "require('./deps/acorn/acorn/package.json').version")

echo "Comparing $NEW_VERSION with $CURRENT_VERSION"

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because Acorn is on the latest version."
  exit 0
fi

cd "$( dirname "$0" )/../.." || exit

rm -rf deps/acorn/acorn

(
    rm -rf acorn-tmp
    mkdir acorn-tmp
    cd acorn-tmp || exit

    "$NODE" "$NPM" init --yes

    "$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts acorn
    cd node_modules/acorn
    # update this version information in src/acorn_version.h
    FILE_PATH="$ROOT/src/acorn_version.h"
    echo "// This is an auto generated file, please do not edit." > "$FILE_PATH"
    echo "// Refer to tools/update-acorn.sh" >> "$FILE_PATH"
    echo "#ifndef SRC_ACORN_VERSION_H_" >> "$FILE_PATH"
    echo "#define SRC_ACORN_VERSION_H_" >> "$FILE_PATH"
    echo "#define ACORN_VERSION \"$NEW_VERSION\"" >> "$FILE_PATH"
    echo "#endif  // SRC_ACORN_VERSION_H_" >> "$FILE_PATH"
)

mv acorn-tmp/node_modules/acorn deps/acorn

rm -rf acorn-tmp/

echo "All done!"
echo ""
echo "Please git add acorn, commit the new version:"
echo ""
echo "$ git add -A deps/acorn"
echo "$ git commit -m \"deps: update acorn to $NEW_VERSION\""
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
