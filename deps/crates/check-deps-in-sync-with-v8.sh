#!/bin/sh
#! nix-shell --pure -i bash -p cacert gawk git jq tomlq
set -e

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)

THIRD_PARTY_REPO=$(mktemp -d)
DEP_VERSIONS=$(mktemp)

cleanup () {
  EXIT_CODE=$?
  rm -f "$DEP_VERSIONS"
  rm -rf "$THIRD_PARTY_REPO"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

tq -f "$BASE_DIR/deps/crates/Cargo.lock" -o json '.package' > "$DEP_VERSIONS"


REV=$(awk -F"'" "/'\\/chromium\\/src\\/third_party\\/rust'/{ print \$8 }" "$BASE_DIR/deps/v8/DEPS")
git clone --depth=1 --revision "$REV" https://chromium.googlesource.com/chromium/src/third_party/rust "$THIRD_PARTY_REPO"

MISMATCH=
for dep in $(tq -f "$BASE_DIR/deps/crates/Cargo.toml" -o json '.dependencies' | jq -r 'keys | .[]'); do
  CURRENT_VERSION=$(jq --arg dep_name "$dep" -r '.[] | select(.name == $dep_name) | .version' < "$DEP_VERSIONS")
  REQUIRED_VERSION=$(grep '^Version: ' "$THIRD_PARTY_REPO/$dep"/*/README.chromium | awk '{ print $2 }')

  [ "$CURRENT_VERSION" = "$REQUIRED_VERSION" ] || {
    echo "$dep should be on version $REQUIRED_VERSION, found $CURRENT_VERSION" >&2
    MISMATCH=1
  }
done

[ -z "$MISMATCH" ] && echo "All versions match" >&2
