#!/bin/sh

# Shell script to update acorn-walk in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

set -ex

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
[ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)
NPM="$ROOT/deps/npm/bin/npm-cli.js"

# shellcheck disable=SC1091
. "$ROOT/tools/dep_updaters/utils.sh"

NEW_VERSION=$("$NODE" "$NPM" view acorn-walk dist-tags.latest)
CURRENT_VERSION=$("$NODE" -p "require('./deps/acorn/acorn-walk/package.json').version")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "acorn-walk" "$NEW_VERSION" "$CURRENT_VERSION"

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

echo "Fetching ada source archive..."

DIST_URL=$(curl -sL "https://registry.npmjs.org/acorn-walk/$NEW_VERSION" | jq -r .dist.tarball)

ACORN_WALK_TGZ="acorn-walk.tgz"

curl -sL -o "$ACORN_WALK_TGZ" "$DIST_URL"

log_and_verify_sha256sum "acorn-walk" "$ACORN_WALK_TGZ"

rm -rf "$DEPS_DIR/acorn/acorn-walk"
mkdir -p "$DEPS_DIR/acorn/acorn-walk"

tar -xf "$ACORN_WALK_TGZ"

mv "$WORKSPACE/package" "$DEPS_DIR/acorn/acorn-walk"

rm "$ACORN_WALK_TGZ"

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
