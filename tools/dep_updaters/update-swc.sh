#!/bin/sh

# Shell script to update swc in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
DEPS_DIR="$BASE_DIR/deps"
NPM="$DEPS_DIR/npm/bin/npm-cli.js"

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION=$("$NODE" "$NPM" view @swc/wasm-typescript dist-tags.latest)

CURRENT_VERSION=$("$NODE" -p "require('./deps/swc/package.json').version")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "swc" "$NEW_VERSION" "$CURRENT_VERSION"

cd "$( dirname "$0" )/../.." || exit

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

cd "$WORKSPACE"

echo "Fetching swc source archive..."

"$NODE" "$NPM" pack "@swc/wasm-typescript@$NEW_VERSION"

swc_TGZ="swc-wasm-typescript-$NEW_VERSION.tgz"

log_and_verify_sha256sum "swc" "$swc_TGZ"

cp ./* "$DEPS_DIR/swc/LICENSE"

rm -r "$DEPS_DIR/swc"/*

tar -xf "$swc_TGZ"

cd package

rm -rf node_modules

mv ./* "$DEPS_DIR/swc"

curl -sL -o "LICENSE" "https://raw.githubusercontent.com/swc-project/swc/HEAD/LICENSE"

mv "LICENSE" "$DEPS_DIR/swc/LICENSE"

# update version information in src/undici_version.h
cat > "$ROOT/src/swc_version.h" <<EOF
// This is an auto generated file, please do not edit.
// Refer to tools/dep_updaters/update-swc.sh
#ifndef SRC_SWC_VERSION_H_
#define SRC_SWC_VERSION_H_
#define SWC_VERSION "$NEW_VERSION"
#endif  // SRC_SWC_VERSION_H_
EOF

echo "All done!"
echo ""
echo "Please git add swc, commit the new version:"
echo ""
echo "$ git add -A deps/swc"
echo "$ git commit -m \"deps: update swc to $NEW_VERSION\""
echo ""

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "swc" "$NEW_VERSION" "src/swc_version.h"
