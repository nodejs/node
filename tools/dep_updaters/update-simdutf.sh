#!/bin/sh
set -e
# Shell script to update simdutf in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/simdutf/simdutf/releases/latest');
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const { tag_name } = await res.json();
console.log(tag_name.replace('v', ''));
EOF
)"
CURRENT_VERSION=$(grep "#define SIMDUTF_VERSION" "$DEPS_DIR/simdutf/simdutf.h" | sed -n "s/^.*VERSION \"\(.*\)\"/\1/p")

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because simdutf is on the latest version."
  exit 0
fi

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

SIMDUTF_REF="v$NEW_VERSION"
SIMDUTF_ZIP="simdutf-$NEW_VERSION.zip"
SIMDUTF_LICENSE="LICENSE-MIT"

cd "$WORKSPACE"

echo "Fetching simdutf source archive..."
curl -sL -o "$SIMDUTF_ZIP" "https://github.com/simdutf/simdutf/releases/download/$SIMDUTF_REF/singleheader.zip"
unzip "$SIMDUTF_ZIP"
rm "$SIMDUTF_ZIP"
rm ./*_demo.cpp

curl -sL -o "$SIMDUTF_LICENSE" "https://raw.githubusercontent.com/simdutf/simdutf/HEAD/LICENSE-MIT"

echo "Replacing existing simdutf (except GYP build files)"
mv "$DEPS_DIR/simdutf/"*.gyp "$DEPS_DIR/simdutf/README.md" "$WORKSPACE/"
rm -rf "$DEPS_DIR/simdutf"
mv "$WORKSPACE" "$DEPS_DIR/simdutf"

echo "All done!"
echo ""
echo "Please git add simdutf, commit the new version:"
echo ""
echo "$ git add -A deps/simdutf"
echo "$ git commit -m \"deps: update simdutf to $NEW_VERSION\""
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
