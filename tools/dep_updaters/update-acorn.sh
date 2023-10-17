#!/bin/sh

# Shell script to update acorn in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
[ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM="$ROOT/deps/npm/bin/npm-cli.js"

# shellcheck disable=SC1091
. "$ROOT/tools/dep_updaters/utils.sh"

NEW_VERSION=$("$NODE" "$NPM" view acorn dist-tags.latest)
CURRENT_VERSION=$("$NODE" -p "require('./deps/acorn/acorn/package.json').version")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "acorn" "$NEW_VERSION" "$CURRENT_VERSION"

cd "$( dirname "$0" )/../.." || exit

rm -rf deps/acorn/acorn

(
    rm -rf acorn-tmp
    mkdir acorn-tmp
    cd acorn-tmp || exit

    "$NODE" "$NPM" init --yes

    "$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts "acorn@$NEW_VERSION"
)

# update version information in src/acorn_version.h
cat > "$ROOT/src/acorn_version.h" <<EOF
// This is an auto generated file, please do not edit.
// Refer to tools/dep_updaters/update-acorn.sh
#ifndef SRC_ACORN_VERSION_H_
#define SRC_ACORN_VERSION_H_
#define ACORN_VERSION "$NEW_VERSION"
#endif  // SRC_ACORN_VERSION_H_
EOF

mv acorn-tmp/node_modules/acorn deps/acorn

rm -rf acorn-tmp/

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "acorn" "$NEW_VERSION" "src/acorn_version.h"
