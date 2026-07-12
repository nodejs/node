#!/bin/sh
set -e
# Shell script to update lief in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/lief-project/LIEF/releases/latest',
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

CURRENT_VERSION=$(grep "#define LIEF_VERSION" "$DEPS_DIR/LIEF/include/LIEF/version.h" | sed -n "s/^.*VERSION \"\(.*\)-\"/\1/p")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "lief" "$NEW_VERSION" "$CURRENT_VERSION"

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

LIEF_ZIP="lief-$NEW_VERSION.zip"

cd "$WORKSPACE"

echo "Fetching lief source archive..."
curl -sL -o "$LIEF_ZIP" "https://github.com/lief-project/LIEF/archive/refs/tags/$NEW_VERSION.zip"
# log_and_verify_sha256sum "lief" "$LIEF_ZIP"
unzip "$LIEF_ZIP"
rm "$LIEF_ZIP"

# Prepare third-party and configured headers in-place so build files don't need generation steps
python3 "$BASE_DIR/tools/prepare_lief.py" --source-dir "$WORKSPACE/LIEF-$NEW_VERSION" --lief-dir "$WORKSPACE/LIEF" --version "$NEW_VERSION" || {
  echo "Failed to prepare LIEF third-party and headers" >&2
  exit 1
}

echo "Replacing existing LIEF (except GYP and GN build files)"
cp "$DEPS_DIR/LIEF/"*.gyp "$WORKSPACE/LIEF/"

rm -rf "$DEPS_DIR/LIEF"
mv "$WORKSPACE/LIEF" "$DEPS_DIR/LIEF"

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "lief" "$NEW_VERSION"
