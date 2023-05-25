#!/bin/sh
set -e
# Shell script to update ada in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
ADA_VERSION=$1

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

if [ "$#" -le 0 ]; then
  echo "Error: please provide an ada version to update to"
  echo "	e.g. $0 1.0.0"
  exit 1
fi

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

ADA_REF="v$ADA_VERSION"
ADA_ZIP="ada-$ADA_REF.zip"
ADA_LICENSE="LICENSE-MIT"

cd "$WORKSPACE"

echo "Fetching ada source archive..."
curl -sL -o "$ADA_ZIP" "https://github.com/ada-url/ada/releases/download/$ADA_REF/singleheader.zip"
log_and_verify_sha256sum "ada" "$ADA_ZIP"
unzip "$ADA_ZIP"
rm "$ADA_ZIP"

curl -sL -o "$ADA_LICENSE" "https://raw.githubusercontent.com/ada-url/ada/HEAD/LICENSE-MIT"

echo "Replacing existing ada (except GYP build files)"
mv "$DEPS_DIR/ada/"*.gyp "$DEPS_DIR/ada/README.md" "$WORKSPACE/"
rm -rf "$DEPS_DIR/ada"
mv "$WORKSPACE" "$DEPS_DIR/ada"

echo "All done!"
echo ""
echo "Please git add ada, commit the new version:"
echo ""
echo "$ git add -A deps/ada"
echo "$ git commit -m \"deps: update ada to $ADA_VERSION\""
echo ""
