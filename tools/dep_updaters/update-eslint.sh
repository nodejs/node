#!/bin/sh

# Shell script to update ESLint in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

ROOT=$(cd "$(dirname "$0")/../.." && pwd)

[ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM="$ROOT/deps/npm/bin/npm-cli.js"

# shellcheck disable=SC1091
. "$ROOT/tools/dep_updaters/utils.sh"

NEW_VERSION=$("$NODE" "$NPM" view eslint dist-tags.latest)
CURRENT_VERSION=$("$NODE" -p "require('./tools/node_modules/eslint/package.json').version")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "eslint" "$NEW_VERSION" "$CURRENT_VERSION"

cd "$( dirname "$0" )" || exit
rm -rf ../node_modules/eslint
(
    rm -rf eslint-tmp
    mkdir eslint-tmp
    cd eslint-tmp || exit

    "$NODE" "$NPM" init --yes

    "$NODE" "$NPM" install \
    --ignore-scripts \
    --install-strategy=shallow \
    --no-bin-links \
    "eslint@$NEW_VERSION"
    # Uninstall plugins that we want to install so that they are removed from
    # devDependencies. Otherwise --omit=dev will cause them to be skipped.
    (
        cd node_modules/eslint
        "$NODE" "$NPM" uninstall \
        --install-links=false \
        --ignore-scripts \
        eslint-plugin-jsdoc \
        eslint-plugin-markdown \
        @babel/core \
        @babel/eslint-parser \
        @babel/plugin-syntax-import-attributes
    )
    (
        cd node_modules/eslint
        "$NODE" "$NPM" install \
        --ignore-scripts \
        --install-links=false \
        --no-bin-links \
        --no-save \
        --omit=dev \
        --omit=peer \
        eslint-plugin-jsdoc \
        eslint-plugin-markdown \
        @babel/core \
        @babel/eslint-parser \
        @babel/plugin-syntax-import-attributes
    )
    # Use dmn to remove some unneeded files.
    "$NODE" "$NPM" exec --package=dmn@2.2.2 --yes -- dmn -f clean
    # TODO: Get this into dmn.
    find node_modules -name .package-lock.json -exec rm {} \;
    find node_modules -name 'README*' -exec rm {} \;
)

mv eslint-tmp/node_modules/eslint ../node_modules/eslint
rm -rf eslint-tmp/

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
