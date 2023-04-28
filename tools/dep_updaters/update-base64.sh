#!/bin/sh
set -e
# Shell script to update base64 in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/aklomp/base64/releases/latest');
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const { tag_name } = await res.json();
console.log(tag_name.replace('v', ''));
EOF
)"

CURRENT_VERSION=$(grep "base64 LANGUAGES C VERSION" ./deps/base64/base64/CMakeLists.txt | sed -n "s/^.*VERSION \(.*\))/\1/p")

echo "Comparing $NEW_VERSION with $CURRENT_VERSION"

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because base64 is on the latest version."
  exit 0
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

echo "Fetching base64 source archive"
curl -sL "https://api.github.com/repos/aklomp/base64/tarball/v$NEW_VERSION" | tar xzf -
mv aklomp-base64-* base64

echo "Replacing existing base64"
rm -rf "$DEPS_DIR/base64/base64"
mv "$WORKSPACE/base64" "$DEPS_DIR/base64/"

# Build configuration is handled by `deps/base64/base64.gyp`, but since `config.h` has to be present for the build
# to work, we create it and leave it empty.
echo "// Intentionally empty" >> "$DEPS_DIR/base64/base64/lib/config.h"

echo "All done!"
echo ""
echo "Please git add base64/base64, commit the new version:"
echo ""
echo "$ git add -A deps/base64/base64"
echo "$ git commit -m \"deps: update base64 to $NEW_VERSION\""
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
