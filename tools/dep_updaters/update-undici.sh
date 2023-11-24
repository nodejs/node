#!/bin/sh

# Shell script to update undici in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
[ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM="$ROOT/deps/npm/bin/npm-cli.js"

# shellcheck disable=SC1091
. "$ROOT/tools/dep_updaters/utils.sh"

NEW_VERSION=$("$NODE" "$NPM" view undici dist-tags.latest)
CURRENT_VERSION=$("$NODE" -p "require('./deps/undici/src/package.json').version")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "undici" "$NEW_VERSION" "$CURRENT_VERSION"

cd "$( dirname "$0" )/../.." || exit
rm -rf deps/undici/src
rm -f deps/undici/undici.js

(
    rm -rf undici-tmp
    mkdir undici-tmp
    cd undici-tmp || exit

    "$NODE" "$NPM" init --yes

    "$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts "undici@$NEW_VERSION"
    cd node_modules/undici
    "$NODE" "$NPM" install --no-bin-link --ignore-scripts
    "$NODE" "$NPM" run build:node
    "$NODE" "$NPM" prune --production
    rm node_modules/.package-lock.json
)

# update version information in src/undici_version.h
cat > "$ROOT/src/undici_version.h" <<EOF
// This is an auto generated file, please do not edit.
// Refer to tools/dep_updaters/update-undici.sh
#ifndef SRC_UNDICI_VERSION_H_
#define SRC_UNDICI_VERSION_H_
#define UNDICI_VERSION "$NEW_VERSION"
#endif  // SRC_UNDICI_VERSION_H_
EOF

mv undici-tmp/node_modules/undici deps/undici/src
mv deps/undici/src/undici-fetch.js deps/undici/undici.js
cp deps/undici/src/LICENSE deps/undici/LICENSE

rm -rf undici-tmp/

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "undici" "$NEW_VERSION" "src/undici_version.h"
