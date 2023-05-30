#!/bin/sh
set -e
# Shell script to update histogram in the source tree to specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/HdrHistogram/HdrHistogram_c/releases/latest');
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const { tag_name } = await res.json();
console.log(tag_name.replace('v', ''));
EOF
)"

CURRENT_VERSION=$(grep "#define HDR_HISTOGRAM_VERSION" ./deps/histogram/include/hdr/hdr_histogram_version.h | sed -n "s/^.*VERSION \"\(.*\)\"/\1/p")

echo "Comparing $NEW_VERSION with $CURRENT_VERSION"

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because histogram is on the latest version."
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

HISTOGRAM_TARBALL="$NEW_VERSION.tar.gz"

cd "$WORKSPACE"

echo "Fetching histogram source archive"

curl -sL -o "$HISTOGRAM_TARBALL" "https://github.com/HdrHistogram/HdrHistogram_c/archive/refs/tags/$HISTOGRAM_TARBALL"

log_and_verify_sha256sum "histogram" "$HISTOGRAM_TARBALL"

gzip -dc "$HISTOGRAM_TARBALL" | tar xf -

rm "$HISTOGRAM_TARBALL"

mv "HdrHistogram_c-$NEW_VERSION" histogram

cp "$WORKSPACE/histogram/include/hdr/hdr_histogram_version.h" "$WORKSPACE/histogram/include/hdr/hdr_histogram.h" "$DEPS_DIR/histogram/include/hdr"

cp "$WORKSPACE/histogram/src/hdr_atomic.h" "$WORKSPACE/histogram/src/hdr_malloc.h" "$WORKSPACE/histogram/src/hdr_tests.h" "$WORKSPACE/histogram/src/hdr_histogram.c" "$DEPS_DIR/histogram/src"


echo "All done!"
echo ""
echo "Please git add histogram, commit the new version:"
echo ""
echo "$ git add -A deps/histogram"
echo "$ git commit -m \"deps: update histogram to $NEW_VERSION\""
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
