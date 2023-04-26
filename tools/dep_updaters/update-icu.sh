#!/bin/sh
set -e
# Shell script to update icu in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
TOOLS_DIR="$BASE_DIR/tools"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/unicode-org/icu/releases/latest');
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const { tag_name } = await res.json();
console.log(tag_name.replace('release-', '').replace('-','.'));
EOF
)"

ICU_VERSION_H="$DEPS_DIR/icu-small/source/common/unicode/uvernum.h"

CURRENT_VERSION="$(grep "#define U_ICU_VERSION " "$ICU_VERSION_H" | cut -d'"' -f2)"

echo "Comparing $NEW_VERSION with $CURRENT_VERSION"

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because icu is on the latest version."
  exit 0
fi

NEW_MAJOR_VERSION=$(echo "$NEW_VERSION" | cut -d '.' -f1)
NEW_MINOR_VERSION=$(echo "$NEW_VERSION" | cut -d '.' -f2)

NEW_VERSION_TGZ="https://github.com/unicode-org/icu/releases/download/release-${NEW_VERSION}/icu4c-${NEW_MAJOR_VERSION}_${NEW_MINOR_VERSION}-src.tgz"

./configure --with-intl=full-icu --with-icu-source="$NEW_VERSION_TGZ"

"$TOOLS_DIR/icu/shrink-icu-src.py"

rm -rf "$DEPS_DIR/icu"

CHECKSUM=$(curl -s "$NEW_VERSION_TGZ" | md5sum | cut -d ' ' -f1)

sed -i '' -e "s|\"url\": \"\(.*\)\".*|\"url\": \"$NEW_VERSION_TGZ\",|" "$TOOLS_DIR/icu/current_ver.dep"

sed -i '' -e "s|\"md5\": \"\(.*\)\".*|\"md5\": \"$CHECKSUM\"|" "$TOOLS_DIR/icu/current_ver.dep" 

rm -rf out "$DEPS_DIR/icu" "$DEPS_DIR/icu4c*"

echo "All done!"
echo ""
echo "Please git add icu, commit the new version:"
echo ""
echo "$ git add -A deps/icu-small"
echo "$ git add tools/icu/current_ver.dep"
echo "$ git commit -m \"deps: update icu to $NEW_VERSION\""
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
