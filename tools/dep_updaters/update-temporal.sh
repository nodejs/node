#!/bin/sh
set -e
# Shell script to update temporal in the source tree to specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/boa-dev/temporal/releases/latest');
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const { tag_name } = await res.json();
console.log(tag_name.replace('v', ''));
EOF
)"

CURRENT_VERSION=$(grep -m 1 version ./deps/temporal/Cargo.toml | tr -s ' ' | tr -d '"' | tr -d "'" | cut -d' ' -f3)

# This function exit with 0 if new version and current version are the same
compare_dependency_version "temporal" "$NEW_VERSION" "$CURRENT_VERSION"

echo "Making temporary workspace"

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

TEMPORAL_TARBALL="$NEW_VERSION.tar.gz"

cd "$WORKSPACE"

curl -sL -o "$TEMPORAL_TARBALL"  "https://github.com/boa-dev/temporal/archive/refs/tags/v$NEW_VERSION.tar.gz"
log_and_verify_sha256sum "temporal" "$TEMPORAL_TARBALL"

gzip -dc "$TEMPORAL_TARBALL" | tar xf -

rm "$TEMPORAL_TARBALL"

mv "temporal-$NEW_VERSION" temporal

rm -rf "$WORKSPACE/temporal/.github"

echo "Copying existing gyp files"
cp "$BASE_DIR/deps/temporal/temporal_capi/temporal_capi.gyp" "$WORKSPACE/temporal/temporal_capi"

echo "Applying dependency update"
rm -rf "$BASE_DIR/deps/temporal"
mv "$WORKSPACE/temporal" "$BASE_DIR/deps/temporal"

echo "All done!"
echo ""
echo "Please git add temporal and commit the new version:"
echo ""
echo "$ git add -A deps/temporal"
echo "$ git commit -m \"deps: update temporal to $NEW_VERSION\""
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
