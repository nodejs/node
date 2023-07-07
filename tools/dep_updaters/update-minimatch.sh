#!/bin/sh

# Shell script to update minimatch in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
[ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM="$ROOT/deps/npm/bin/npm-cli.js"

# shellcheck disable=SC1091
. "$ROOT/tools/dep_updaters/utils.sh"

NEW_VERSION=$("$NODE" "$NPM" view minimatch dist-tags.latest)
CURRENT_VERSION=$("$NODE" -p "require('./deps/minimatch/src/package.json').version")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "minimatch" "$NEW_VERSION" "$CURRENT_VERSION"

cd "$( dirname "$0" )/../.." || exit

rm -rf deps/minimatch/src
rm -rf deps/minimatch/index.js

(
    rm -rf minimatch-tmp
    mkdir minimatch-tmp
    cd minimatch-tmp || exit

    "$NODE" "$NPM" init --yes

    "$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts "minimatch@$NEW_VERSION"
    cd node_modules/minimatch
    "$NODE" "$NPM" exec --package=esbuild@0.17.15 --yes -- esbuild ./dist/cjs/index.js --bundle --platform=node --outfile=minimatch.js
)

ls -l minimatch-tmp
mv minimatch-tmp/node_modules/minimatch deps/minimatch/src
mv deps/minimatch/src/minimatch.js deps/minimatch/index.js
cp deps/minimatch/src/LICENSE deps/minimatch/LICENSE

rm -rf minimatch-tmp/

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "minimatch" "$NEW_VERSION"
