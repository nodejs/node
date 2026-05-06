#!/bin/sh
set -e
# Shell script to update ata in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/ata-core/ata-validator/releases/latest',
  process.env.GITHUB_TOKEN && {
    headers: {
      "Authorization": `Bearer ${process.env.GITHUB_TOKEN}`
    },
  });
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const { tag_name } = await res.json();
console.log(tag_name.replace('v', ''));
EOF
)"

CURRENT_VERSION=$(grep "#define ATA_VERSION" "$DEPS_DIR/ata/ata.h" | sed -n "s/^.*VERSION \"\(.*\)\"/\1/p")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "ata" "$NEW_VERSION" "$CURRENT_VERSION"

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

ATA_REF="v$NEW_VERSION"
ATA_ZIP="ata-$NEW_VERSION.zip"

cd "$WORKSPACE"

echo "Fetching ata source archive..."
curl -sL -o "$ATA_ZIP" "https://github.com/ata-core/ata-validator/archive/refs/tags/$ATA_REF.zip"
unzip "$ATA_ZIP"
cd "ata-validator-$NEW_VERSION"

echo "Replacing existing ata (except GYP build files)"
mv "$DEPS_DIR/ata/ata.gyp" "$WORKSPACE/"
rm -rf "$DEPS_DIR/ata"
mkdir -p "$DEPS_DIR/ata"
mv singleheader/ata.h "$DEPS_DIR/ata/"
mv singleheader/ata.cpp "$DEPS_DIR/ata/"
mv LICENSE "$DEPS_DIR/ata/"
mv "$WORKSPACE/ata.gyp" "$DEPS_DIR/ata/"

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "ata" "$NEW_VERSION"
