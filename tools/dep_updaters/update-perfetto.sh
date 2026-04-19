#!/bin/sh
set -e
# Shell script to update perfetto in the source tree to specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/google/perfetto/releases/latest',
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

CURRENT_VERSION=$(cat ./deps/perfetto/VERSION)

# This function exit with 0 if new version and current version are the same
compare_dependency_version "perfetto" "$NEW_VERSION" "$CURRENT_VERSION"

echo "Making temporary workspace"

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

PERFETTO_REF="v$NEW_VERSION"
PERFETTO_SDK_ZIP="perfetto-cpp-sdk-src.zip"

cd "$WORKSPACE"

echo "Fetching perfetto sdk archive"
curl -sL -o "$PERFETTO_SDK_ZIP" "https://github.com/google/perfetto/releases/download/$PERFETTO_REF/$PERFETTO_SDK_ZIP"

echo "Unpacking archive"
mkdir -p perfetto/sdk
unzip -d perfetto/sdk "$PERFETTO_SDK_ZIP"
rm "$PERFETTO_SDK_ZIP"
echo "$NEW_VERSION" > perfetto/VERSION
curl -sL -o "perfetto/LICENSE" "https://raw.githubusercontent.com/google/perfetto/refs/tags/$PERFETTO_REF/LICENSE"

# Remove C API headers. Only keep C++ API headers.
rm perfetto/sdk/perfetto_c.h perfetto/sdk/perfetto_c.cc

echo "Copying existing gyp files"
cp "$DEPS_DIR/perfetto/perfetto.gyp" "$WORKSPACE/perfetto"

echo "Copying existing GN files"
cp "$DEPS_DIR/perfetto/"*.gn "$DEPS_DIR/perfetto/"*.gni "$WORKSPACE/perfetto"

echo "Replacing existing perfetto"
rm -rf "$DEPS_DIR/perfetto"
mv "$WORKSPACE/perfetto" "$DEPS_DIR/"

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "perfetto" "$NEW_VERSION"
