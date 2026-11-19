#!/bin/sh
set -e
# Shell script to update Jsonifier in the source tree to the head of its node branch

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
JSONIFIER_REPO="nihilai-collective/Jsonifier"
JSONIFIER_BRANCH="node"

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

cd "$WORKSPACE"

curl -sfL -o Version "https://raw.githubusercontent.com/$JSONIFIER_REPO/$JSONIFIER_BRANCH/Version"
NEW_VERSION=$(tr -d '\r\n ' < Version)
CURRENT_VERSION=$(tr -d '\r\n ' < "$DEPS_DIR/jsonifier/VERSION")

if [ -z "$NEW_VERSION" ]; then
  echo "Could not read the Jsonifier version from the $JSONIFIER_BRANCH branch."
  exit 1
fi

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ] && [ -z "$FORCE" ]; then
  echo "Skipped because Jsonifier is on the latest version."
  exit 0
fi

echo "Fetching Jsonifier source archive..."
curl -sfL -o jsonifier.tar.gz "https://github.com/$JSONIFIER_REPO/archive/refs/heads/$JSONIFIER_BRANCH.tar.gz"
tar -xzf jsonifier.tar.gz
cd "Jsonifier-$JSONIFIER_BRANCH"

rm -rf "$DEPS_DIR/jsonifier/include"
mv include "$DEPS_DIR/jsonifier/include"
mv License.md "$DEPS_DIR/jsonifier/LICENSE"
mv Third_Party_Licenses.md "$DEPS_DIR/jsonifier/THIRD_PARTY_LICENSES.md"
mv Version "$DEPS_DIR/jsonifier/VERSION"

# update version information in src/jsonifier_version.h
cat > "$BASE_DIR/src/jsonifier_version.h" <<EOF
// This is an auto generated file, please do not edit.
// Refer to tools/dep_updaters/update-jsonifier.sh
#ifndef SRC_JSONIFIER_VERSION_H_
#define SRC_JSONIFIER_VERSION_H_
#define JSONIFIER_VERSION "$NEW_VERSION"
#endif  // SRC_JSONIFIER_VERSION_H_
EOF

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "jsonifier" "$NEW_VERSION" "src/jsonifier_version.h"
