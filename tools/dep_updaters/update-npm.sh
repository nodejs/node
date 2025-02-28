#!/bin/sh
set -e
# Shell script to update npm in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NPM="$DEPS_DIR/npm/bin/npm-cli.js"

NPM_VERSION=$1

if [ "$#" -le 0 ]; then
  echo "Error: please provide an npm version to update to"
  exit 1
fi

echo "Making temporary workspace"

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

cd "$WORKSPACE"

NPM_TGZ="npm-v$NPM_VERSION.tar.gz"

NPM_TARBALL="$($NODE "$NPM" view npm@"$NPM_VERSION" dist.tarball)"

curl -s "$NPM_TARBALL" > "$NPM_TGZ"

log_and_verify_sha256sum "npm" "$NPM_TGZ"

rm -rf "$DEPS_DIR/npm"

mkdir "$DEPS_DIR/npm"

tar zxvf "$NPM_TGZ" --strip-component=1 -C "$DEPS_DIR/npm"

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "npm" "$NPM_VERSION"
