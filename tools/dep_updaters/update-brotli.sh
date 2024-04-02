#!/bin/sh
set -e
# Shell script to update brotli in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/google/brotli/releases/latest');
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const { tag_name } = await res.json();
console.log(tag_name.replace('v', ''));
EOF
)"

VERSION_HEX=$(grep "#define BROTLI_VERSION" ./deps/brotli/c/common/version.h | sed 's/.* //')

major=$(( ($VERSION_HEX >> 24) & 0xff ))
minor=$(( ($VERSION_HEX >> 12) & 0xfff ))
patch=$(( $VERSION_HEX & 0xfff ))
CURRENT_VERSION="${major}.${minor}.${patch}"

echo "Comparing $NEW_VERSION with $CURRENT_VERSION"

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because brotli is on the latest version."
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

BROTLI_TARBALL="brotli-v$NEW_VERSION.tar.gz"

echo "Fetching brotli source archive"
curl -sL -o "$BROTLI_TARBALL" "https://github.com/google/brotli/archive/v$NEW_VERSION.tar.gz"
log_and_verify_sha256sum "brotli" "$BROTLI_TARBALL"
gzip -dc "$BROTLI_TARBALL" | tar xf -
rm "$BROTLI_TARBALL"
mv "brotli-$NEW_VERSION" "brotli"

echo "Copying existing gyp file"
cp "$DEPS_DIR/brotli/brotli.gyp" "$WORKSPACE/brotli"

echo "Deleting existing brotli"
rm -rf "$DEPS_DIR/brotli"
mkdir "$DEPS_DIR/brotli"

echo "Update c and LICENSE"
mv "$WORKSPACE/brotli/c" "$WORKSPACE/brotli/LICENSE" "$WORKSPACE/brotli/brotli.gyp" "$DEPS_DIR/brotli"

echo "All done!"
echo ""
echo "Please git add brotli, commit the new version:"
echo ""
echo "$ git add -A deps/brotli"
echo "$ git commit -m \"deps: update brotli to $NEW_VERSION\""
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
