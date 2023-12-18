#!/bin/sh
set -e
# Shell script to update v8 patch update

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

cd "$BASE_DIR"

IS_UP_TO_DATE=$(git node v8 minor | grep "V8 is up-to-date")

if [ -n "$IS_UP_TO_DATE" ]; then
  echo "Skipped because V8 is on the latest version."
  exit 0
fi

DEPS_DIR="$BASE_DIR/deps"

CURRENT_MAJOR_VERSION=$(grep "#define V8_MAJOR_VERSION" "$DEPS_DIR/v8/include/v8-version.h" | cut -d ' ' -f3)
CURRENT_MINOR_VERSION=$(grep "#define V8_MINOR_VERSION" "$DEPS_DIR/v8/include/v8-version.h" | cut -d ' ' -f3)
CURRENT_BUILD_VERSION=$(grep "#define V8_BUILD_NUMBER" "$DEPS_DIR/v8/include/v8-version.h" | cut -d ' ' -f3)
CURRENT_PATCH_VERSION=$(grep "#define V8_PATCH_LEVEL" "$DEPS_DIR/v8/include/v8-version.h" | cut -d ' ' -f3)

NEW_VERSION="$CURRENT_MAJOR_VERSION.$CURRENT_MINOR_VERSION.$CURRENT_BUILD_VERSION.$CURRENT_PATCH_VERSION"


echo "All done!"
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
