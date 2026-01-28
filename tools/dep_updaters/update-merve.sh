#!/bin/sh
set -e
# Shell script to update merve in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/anonrig/merve/releases/latest',
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

CURRENT_VERSION=$(grep "#define MERVE_VERSION" "$DEPS_DIR/merve/merve.h" | sed -n "s/^.*VERSION \"\(.*\)\"/\1/p")

# This function exit with 0 if new version and current version are the same
compare_dependency_version "merve" "$NEW_VERSION" "$CURRENT_VERSION"

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

MERVE_REF="v$NEW_VERSION"
MERVE_ZIP="singleheader.zip"
MERVE_LICENSE="LICENSE-MIT"

cd "$WORKSPACE"

echo "Fetching merve source archive..."
curl -sL -o "$MERVE_ZIP" "https://github.com/anonrig/merve/releases/download/$MERVE_REF/singleheader.zip"
log_and_verify_sha256sum "merve" "$MERVE_ZIP"
unzip "$MERVE_ZIP"
rm "$MERVE_ZIP"

curl -sL -o "$MERVE_LICENSE" "https://raw.githubusercontent.com/anonrig/merve/HEAD/LICENSE-MIT"

echo "Replacing existing merve (except GYP build files)"
mv "$DEPS_DIR/merve/merve.gyp" "$WORKSPACE/"
rm -rf "$DEPS_DIR/merve"
mv "$WORKSPACE" "$DEPS_DIR/merve"

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "merve" "$NEW_VERSION"
