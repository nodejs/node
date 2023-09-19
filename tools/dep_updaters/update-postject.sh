#!/bin/sh

# Shell script to update postject in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
[ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM="$ROOT/deps/npm/bin/npm-cli.js"

# shellcheck disable=SC1091
. "$ROOT/tools/dep_updaters/utils.sh"

NEW_VERSION=$("$NODE" "$NPM" view postject dist-tags.latest)
CURRENT_VERSION=$("$NODE" -p "require('./test/fixtures/postject-copy/node_modules/postject/package.json').version")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "postject" "$NEW_VERSION" "$CURRENT_VERSION"

cd "$( dirname "$0" )/../.." || exit
rm -rf test/fixtures/postject-copy
mkdir test/fixtures/postject-copy
cd test/fixtures/postject-copy || exit

"$NODE" "$NPM" init --yes

"$NODE" "$NPM" install --no-bin-links --ignore-scripts "postject@$NEW_VERSION"

# TODO(RaisinTen): Replace following with $WORKSPACE
cd ../../..
rm -rf deps/postject
mkdir deps/postject
cp test/fixtures/postject-copy/node_modules/postject/LICENSE deps/postject
cp test/fixtures/postject-copy/node_modules/postject/dist/postject-api.h deps/postject

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "postject" "$NEW_VERSION"
