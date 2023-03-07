#!/bin/sh
set -e
# Shell script to update c-ares in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/c-ares/c-ares/releases/latest');
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const { tag_name } = await res.json();
console.log(tag_name.replace('cares-', '').replaceAll('_', '.'));
EOF
)"

CURRENT_VERSION=$(grep "#define ARES_VERSION_STR" ./deps/cares/include/ares_version.h |  sed -n "s/^.*VERSION_STR \"\(.*\)\"/\1/p")

echo "Comparing $NEW_VERSION with $CURRENT_VERSION"

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because c-ares is on the latest version."
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

ARES_REF="cares-$(echo "$NEW_VERSION" | tr . _)"
ARES_TARBALL="c-ares-$NEW_VERSION.tar.gz"

cd "$WORKSPACE"

echo "Fetching c-ares source archive"
curl -sL "https://github.com/c-ares/c-ares/releases/download/$ARES_REF/$ARES_TARBALL" | tar xz
mv "c-ares-$NEW_VERSION" cares

echo "Removing tests"
rm -rf "$WORKSPACE/cares/test"

echo "Copying existing .gitignore, config and gyp files"
cp -R "$DEPS_DIR/cares/config" "$WORKSPACE/cares"
cp "$DEPS_DIR/cares/.gitignore" "$WORKSPACE/cares"
cp "$DEPS_DIR/cares/cares.gyp" "$WORKSPACE/cares"

echo "Replacing existing c-ares"
rm -rf "$DEPS_DIR/cares"
mv "$WORKSPACE/cares" "$DEPS_DIR/"

echo "All done!"
echo ""
echo "Please git add c-ares, commit the new version:"
echo ""
echo "$ git add -A deps/cares"
echo "$ git commit -m \"deps: update c-ares to $NEW_VERSION\""
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
