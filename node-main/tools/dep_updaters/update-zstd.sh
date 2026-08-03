#!/bin/sh
set -e
# Shell script to update zstd in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/facebook/zstd/releases/latest',
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

CURRENT_MAJOR_VERSION=$(grep "#define ZSTD_VERSION_MAJOR" ./deps/zstd/lib/zstd.h | sed -n "s/^.*MAJOR[[:space:]]\{1,\}\([[:digit:]]\{1,\}\)/\1/p")
CURRENT_MINOR_VERSION=$(grep "#define ZSTD_VERSION_MINOR" ./deps/zstd/lib/zstd.h | sed -n "s/^.*MINOR[[:space:]]\{1,\}\([[:digit:]]\{1,\}\)/\1/p")
CURRENT_PATCH_VERSION=$(grep "#define ZSTD_VERSION_RELEASE" ./deps/zstd/lib/zstd.h | sed -n "s/^.*RELEASE[[:space:]]\{1,\}\([[:digit:]]\{1,\}\)/\1/p")
CURRENT_VERSION="$CURRENT_MAJOR_VERSION.$CURRENT_MINOR_VERSION.$CURRENT_PATCH_VERSION"

# This function exit with 0 if new version and current version are the same
compare_dependency_version "zstd" "$NEW_VERSION" "$CURRENT_VERSION"

echo "Making temporary workspace"

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

cd "$WORKSPACE"

zstd_TARBALL="zstd-v$NEW_VERSION.tar.gz"

echo "Fetching zstd source archive"
curl -sL -o "$zstd_TARBALL" "https://github.com/facebook/zstd/archive/v$NEW_VERSION.tar.gz"
log_and_verify_sha256sum "zstd" "$zstd_TARBALL"
gzip -dc "$zstd_TARBALL" | tar xf -
rm "$zstd_TARBALL"
mv "zstd-$NEW_VERSION" "zstd"

echo "Copying existing gyp file"
cp "$DEPS_DIR/zstd/zstd.gyp" "$WORKSPACE/zstd"

echo "Copying existing GN files"
cp "$DEPS_DIR/zstd/"*.gn "$DEPS_DIR/zstd/"*.gni "$WORKSPACE/zstd"

echo "Deleting existing zstd"
rm -rf "$DEPS_DIR/zstd"
mkdir "$DEPS_DIR/zstd"

echo "Update c and LICENSE"
mv "$WORKSPACE/zstd/"*.gn "$WORKSPACE/zstd/"*.gni "$WORKSPACE/zstd/lib" "$WORKSPACE/zstd/LICENSE" "$WORKSPACE/zstd/zstd.gyp" "$DEPS_DIR/zstd"

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "zstd" "$NEW_VERSION"
