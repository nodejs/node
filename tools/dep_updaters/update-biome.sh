#!/bin/sh

# Shell script to update @biomejs/biome in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

ROOT=$(cd "$(dirname "$0")/../.." && pwd)

[ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM="$ROOT/deps/npm/bin/npm-cli.js"

# shellcheck disable=SC1091
. "$ROOT/tools/dep_updaters/utils.sh"

NEW_VERSION=$("$NODE" "$NPM" view @biomejs/biome dist-tags.latest)
CURRENT_VERSION=$("$NODE" -p "require('./tools/biome/package.json').dependencies['@biomejs/biome']")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "@biomejs/biome" "$NEW_VERSION" "$CURRENT_VERSION"

cd tools/biome || exit
"$NODE" "$NPM" install "@biomejs/biome@$NEW_VERSION"

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
