#!/bin/sh
set -e
# Shell script to update base64 in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

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

BASE64_TARBALL="base64-v$NEW_VERSION.tar.gz"

echo "Fetching base64 source archive"
curl -sL -o "$BASE64_TARBALL" "https://api.github.com/repos/aklomp/base64/tarball/v$NEW_VERSION"
log_and_verify_sha256sum "base64" "$BASE64_TARBALL"
gzip -dc "$BASE64_TARBALL" | tar xf -
rm "$BASE64_TARBALL"
mv aklomp-base64-* base64

echo "Replacing existing base64"
rm -rf "$DEPS_DIR/base64/base64"
mv "$WORKSPACE/base64" "$DEPS_DIR/base64/"

# Build configuration is handled by `deps/base64/base64.gyp`, but since `config.h` has to be present for the build
# to work, we create it and leave it empty.
echo "// Intentionally empty" > "$DEPS_DIR/base64/base64/lib/config.h"

# Clear out .gitignore, otherwise config.h is ignored. That's dangerous when
# people check in our tarballs into source control and run `git clean`.
echo "# Intentionally empty" > "$DEPS_DIR/base64/base64/.gitignore"

echo "All done!"
echo ""
echo "Please git add base64/base64, commit the new version:"
echo ""
echo "$ git add -A deps/base64/base64 src/base64_version.h"
echo "$ git commit -m \"deps: update base64 to $NEW_VERSION\""
echo ""

# update the base64_version.h
cat > "$BASE_DIR/src/base64_version.h" << EOL
// This is an auto generated file, please do not edit.
// Refer to tools/dep_updaters/update-base64.sh
#ifndef SRC_BASE64_VERSION_H_
#define SRC_BASE64_VERSION_H_
#define BASE64_VERSION "$NEW_VERSION"
#endif  // SRC_BASE64_VERSION_H_
EOL

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
