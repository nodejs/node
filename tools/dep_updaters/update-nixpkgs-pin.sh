#!/bin/sh
set -ex
# Shell script to update Nixpkgs pin in the source tree to the most recent
# version on the unstable channel.

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
NIXPKGS_PIN_FILE="$BASE_DIR/tools/nix/pkgs.nix"

NIXPKGS_REPO=$(grep 'repo =' "$NIXPKGS_PIN_FILE" | awk -F'"' '{ print $2 }')
CURRENT_VERSION_SHA1=$(grep 'rev =' "$NIXPKGS_PIN_FILE" | awk -F'"' '{ print $2 }')
CURRENT_VERSION=$(echo "$CURRENT_VERSION_SHA1" | head -c 7)

NEW_UPSTREAM_SHA1=$(git ls-remote "$NIXPKGS_REPO.git" nixpkgs-unstable | awk '{print $1}')
NEW_VERSION=$(echo "$NEW_UPSTREAM_SHA1" | head -c 7)


# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

compare_dependency_version "nixpkgs-unstable" "$NEW_VERSION" "$CURRENT_VERSION"

CURRENT_TARBALL_HASH=$(grep 'sha256 =' "$NIXPKGS_PIN_FILE" | awk -F'"' '{ print $2 }')
NEW_TARBALL_HASH=$(nix-prefetch-url --unpack "$NIXPKGS_REPO/archive/$NEW_UPSTREAM_SHA1.tar.gz")

TMP_FILE=$(mktemp)
sed "s/$CURRENT_VERSION_SHA1/$NEW_UPSTREAM_SHA1/;s/$CURRENT_TARBALL_HASH/$NEW_TARBALL_HASH/" "$NIXPKGS_PIN_FILE" > "$TMP_FILE"
mv "$TMP_FILE" "$NIXPKGS_PIN_FILE"

cat -<<EOF
All done!

Please git add and commit the new version:

$ git add $NIXPKGS_PIN_FILE
$ git commit -m 'tools: bump nixpkgs-unstable pin to $NEW_VERSION'
EOF

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
