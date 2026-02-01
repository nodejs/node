#!/bin/sh
set -ex
# Shell script to update ncrypto in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/nodejs/ncrypto/releases/latest',
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

CURRENT_VERSION=$(awk -F'"' '/^#define NCRYPTO_VERSION /{ print $2 }' "$DEPS_DIR/ncrypto/ncrypto/version.h" || true)

# This function exit with 0 if new version and current version are the same
compare_dependency_version "ncrypto" "$NEW_VERSION" "$CURRENT_VERSION"

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

echo "Fetching ncrypto source archive..."
NCRYPTO_TARBALL="ncrypto-v$NEW_VERSION.tar.gz"
curl -sL "https://api.github.com/repos/nodejs/ncrypto/tarball/v$NEW_VERSION" \
| tar xz --strip-components=1 -C "$WORKSPACE" --wildcards \
    '*/README.md' \
    '*/src/engine.cpp' \
    '*/src/ncrypto.cpp' \
    '*/include/ncrypto.h' \
    '*/include/ncrypto/version.h'

mv "$WORKSPACE/README.md" "$DEPS_DIR/ncrypto/."
mv "$WORKSPACE/src/engine.cpp" "$DEPS_DIR/ncrypto/engine.cc"
mv "$WORKSPACE/src/ncrypto.cpp" "$DEPS_DIR/ncrypto/ncrypto.cc"
mv "$WORKSPACE/include/"* "$DEPS_DIR/ncrypto/."

cleanup

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "ncrypto" "$NEW_VERSION"
