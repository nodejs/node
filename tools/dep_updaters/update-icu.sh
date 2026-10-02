#!/bin/sh
set -e
# Shell script to update icu in the source tree to a specific version.
# Pass `--update-keys` to update the local copy of the key files after verifying
# the upstream file history.

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"
TOOLS_DIR="$BASE_DIR/tools"

[ -z "$NODE" ] && NODE="$BASE_DIR/out/Release/node"
[ -x "$NODE" ] || NODE=$(command -v node)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

NEW_VERSION="$("$NODE" --input-type=module <<'EOF'
const res = await fetch('https://api.github.com/repos/unicode-org/icu/releases/latest',
  process.env.GITHUB_TOKEN && {
    headers: {
      "Authorization": `Bearer ${process.env.GITHUB_TOKEN}`
    },
  });
if (!res.ok) throw new Error(`FetchError: ${res.status} ${res.statusText}`, { cause: res });
const { tag_name } = await res.json();
console.log(tag_name.replace('release-', '').replace('-','.'));
EOF
)"

ICU_VERSION_H="$DEPS_DIR/icu-small/source/common/unicode/uvernum.h"

CURRENT_VERSION="$(grep "#define U_ICU_VERSION " "$ICU_VERSION_H" | cut -d'"' -f2)"

# This function exit with 0 if new version and current version are the same
compare_dependency_version "icu-small" "$NEW_VERSION" "$CURRENT_VERSION"

NEW_VERSION_TGZ="icu4c-${NEW_VERSION}-sources.tgz"

NEW_VERSION_TGZ_URL="https://github.com/unicode-org/icu/releases/download/release-${NEW_VERSION}/${NEW_VERSION_TGZ}"

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')
NEW_VERSION_TGZ_PATH="$WORKSPACE/$NEW_VERSION_TGZ"

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

echo "Fetching ICU source archive"
curl -sfL -o "$NEW_VERSION_TGZ_PATH" "$NEW_VERSION_TGZ_URL"

KEYRING="$BASE_DIR/tools/dep_updaters/icu.kbx"
if [ "$1" = "--update-keys" ]; then
  KEYS_FILE="$WORKSPACE/KEYS"
  echo "Fetching the upstream KEYS file"
  curl -sSLfo "$KEYS_FILE" https://github.com/unicode-org/icu/raw/refs/tags/release-${NEW_VERSION}/KEYS
  rm -f "$KEYRING"
  gpg --no-default-keyring --keyring "$KEYRING" --batch --import --import-options import-minimal < "$KEYS_FILE"
fi

echo "Verifying PGP signature"
curl -sfL -o "$NEW_VERSION_TGZ_PATH.asc" "$NEW_VERSION_TGZ_URL.asc"
gpgv --keyring "$KEYRING" "$NEW_VERSION_TGZ_PATH.asc" "$NEW_VERSION_TGZ_PATH"

CHECKSUM=$(shasum -a 256 "$NEW_VERSION_TGZ_PATH" | cut -d ' ' -f1)
echo "sha256: $CHECKSUM"

./configure --with-intl=full-icu --with-icu-source="$NEW_VERSION_TGZ_PATH"

"$TOOLS_DIR/icu/shrink-icu-src.py"

rm -rf "$DEPS_DIR/icu"

URL="$NEW_VERSION_TGZ_URL" SHA256="$CHECKSUM" "$NODE" -e '
  const { URL: url, SHA256: sha256 } = process.env;
  console.log(JSON.stringify([{ url, sha256 }], null, 2));
' > "$TOOLS_DIR/icu/current_ver.dep"

rm -rf out "$DEPS_DIR/icu" "$DEPS_DIR/icu4c*"

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "icu-small" "$NEW_VERSION" "tools/icu/current_ver.dep"
