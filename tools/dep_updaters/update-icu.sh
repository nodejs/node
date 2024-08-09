#!/bin/sh
set -e
# Shell script to update icu in the source tree to a specific version

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

DASHED_NEW_VERSION=$(echo "$NEW_VERSION" | sed 's/\./-/g')

LOW_DASHED_NEW_VERSION=$(echo "$NEW_VERSION" | sed 's/\./_/g')

NEW_VERSION_TGZ="icu4c-${LOW_DASHED_NEW_VERSION}-src.tgz"

NEW_VERSION_TGZ_URL="https://github.com/unicode-org/icu/releases/download/release-${DASHED_NEW_VERSION}/$NEW_VERSION_TGZ"

NEW_VERSION_MD5="https://github.com/unicode-org/icu/releases/download/release-${DASHED_NEW_VERSION}/icu4c-${LOW_DASHED_NEW_VERSION}-sources.md5"
NEW_VERSION_TGZ_ASC_URL="https://github.com/unicode-org/icu/releases/download/release-${DASHED_NEW_VERSION}/icu4c-${LOW_DASHED_NEW_VERSION}-src.tgz.asc"

KEY_URL="https://raw.githubusercontent.com/unicode-org/icu/release-$(echo "$NEW_VERSION" | sed 's/\./-/')/KEYS"


CHECKSUM=$(curl -sL "$NEW_VERSION_MD5" | grep "$NEW_VERSION_TGZ" | grep -v "\.asc$" | awk '{print $1}')

MD5_CHECK_AVAILABLE=$( [ -n "$CHECKSUM" ] && echo true || echo false )
GPG_COMMAND=$(command -v gpg)
GPG_CHECK_AVAILABLE=$( [ -n "$GPG_COMMAND" ] && echo true || echo false )

if [ "$MD5_CHECK_AVAILABLE" = false ] && [ "$GPG_CHECK_AVAILABLE" = false ]; then
  echo "Neither md5 checksum nor gpg command found"
  exit 0
fi 

if $MD5_CHECK_AVAILABLE; then
  GENERATED_CHECKSUM=$( curl -sL "$NEW_VERSION_TGZ_URL" | md5sum | cut -d ' ' -f1)
  echo "Comparing checksums: deposited '$CHECKSUM' with '$GENERATED_CHECKSUM'"
  if [ "$CHECKSUM" != "$GENERATED_CHECKSUM" ]; then
    echo "Skipped because checksums do not match."
    exit 0
  fi
else 
  echo "Checksum not found"
fi 
if $GPG_CHECK_AVAILABLE; then
  echo "check with gpg"
  curl -sL "$KEY_URL" > KEYS
  curl -sL "$NEW_VERSION_TGZ_URL" > data.tgz
  curl -sL "$NEW_VERSION_TGZ_ASC_URL" > signature.asc
  gpg --import KEYS
  if gpg --verify signature.asc data.tgz; then
    echo "Signature verified"
    rm data.tgz signature.asc KEYS

  else
    echo "Skipped because signature verification failed."
    rm data.tgz signature.asc KEYS
    exit 0
  fi
else 
  echo "GPG command not found"
fi

./configure --with-intl=full-icu --with-icu-source="$NEW_VERSION_TGZ_URL"

"$TOOLS_DIR/icu/shrink-icu-src.py"

rm -rf "$DEPS_DIR/icu"

printf "[
  {
    \"url\": \"%s\"" "$NEW_VERSION_TGZ_URL" > "$TOOLS_DIR/icu/current_ver.dep"

if $MD5_CHECK_AVAILABLE; then 
  echo ',' >> "$TOOLS_DIR/icu/current_ver.dep"
  printf "    \"md5\": \"%s\"" "$CHECKSUM" >> "$TOOLS_DIR/icu/current_ver.dep"
fi

if $GPG_CHECK_AVAILABLE; then
  echo ',' >> "$TOOLS_DIR/icu/current_ver.dep"
  printf "    \"gpg\": {
      \"key\": \"%s\",
      \"asc\": \"%s\"
    }" "$KEY_URL" "$NEW_VERSION_TGZ_ASC_URL" >> "$TOOLS_DIR/icu/current_ver.dep"
fi
echo "
  }
]" >> "$TOOLS_DIR/icu/current_ver.dep"

rm -rf out "$DEPS_DIR/icu" "$DEPS_DIR/icu4c*"

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "icu-small" "$NEW_VERSION" "tools/icu/current_ver.dep"
