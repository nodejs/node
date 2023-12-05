#!/bin/sh

# Shell script to update @reporters/github in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
[ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM="$ROOT/deps/npm/bin/npm-cli.js"

# shellcheck disable=SC1091
. "$ROOT/tools/dep_updaters/utils.sh"

NEW_VERSION=$("$NODE" "$NPM" view @reporters/github dist-tags.latest)
CURRENT_VERSION=$("$NODE" -p "require('./tools/github_reporter/package.json').version")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "@reporters/github" "$NEW_VERSION" "$CURRENT_VERSION"

cd "$( dirname "$0" )/../.." || exit
rm -rf tools/github_reporter

(
    rm -rf github-reporter-tmp
    mkdir github-reporter-tmp
    cd github-reporter-tmp || exit

    "$NODE" "$NPM" init --yes
    "$NODE" "$NPM" install --global-style --no-bin-links --ignore-scripts "@reporters/github@$NEW_VERSION"
    "$NODE" "$NPM" exec --package=esbuild@0.17.15 --yes -- esbuild ./node_modules/@reporters/github --bundle --platform=node --outfile=bundle.js
)

mkdir tools/github_reporter
mv github-reporter-tmp/bundle.js tools/github_reporter/index.js
mv github-reporter-tmp/node_modules/@reporters/github/package.json tools/github_reporter/package.json
rm -rf github-reporter-tmp/

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
