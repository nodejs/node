#!/bin/sh
set -ex
# Shell script to update Nixpkgs pin in the source tree to the most recent
# version on the unstable channel, and the 26.05 one for Intel Mac support.

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
NIXPKGS_PIN_FILE="$BASE_DIR/tools/nix/pkgs.nix"
NIXPKGS_COMPAT_PIN_FILE="$BASE_DIR/tools/nix/pkgs-26.05.nix"
OPENSSL_MATRIX_FILE="$BASE_DIR/tools/nix/openssl-matrix.nix"

NIXPKGS_REPO=$(grep 'repo =' "$NIXPKGS_PIN_FILE" | awk -F'"' '{ print $2 }')
CURRENT_VERSION_SHA1=$(grep 'rev =' "$NIXPKGS_PIN_FILE" | awk -F'"' '{ print $2 }')

NEW_UPSTREAM_SHA1=$(git ls-remote "$NIXPKGS_REPO.git" nixpkgs-unstable | awk '{print $1}')
NEW_VERSION=$(echo "$NEW_UPSTREAM_SHA1" | head -c 35)


# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

compare_dependency_version "nixpkgs-unstable" "$CURRENT_VERSION_SHA1" "$NEW_UPSTREAM_SHA1"

update_pkgs_file() {
  PIN_FILE=$1
  PREVIOUS_SHA1=$2
  UPSTREAM_SHA1=$3

  CURRENT_TARBALL_HASH=$(grep 'sha256 =' "$PIN_FILE" | awk -F'"' '{ print $2 }')
  NEW_TARBALL_HASH=$(nix-prefetch-url --unpack "$NIXPKGS_REPO/archive/$UPSTREAM_SHA1.tar.gz")

  TMP_FILE=$(mktemp)
  sed "s/$PREVIOUS_SHA1/$UPSTREAM_SHA1/;s/$CURRENT_TARBALL_HASH/$NEW_TARBALL_HASH/" "$PIN_FILE" > "$TMP_FILE"
  mv "$TMP_FILE" "$PIN_FILE"
}

update_pkgs_file "$NIXPKGS_PIN_FILE" "$CURRENT_VERSION_SHA1" "$NEW_UPSTREAM_SHA1"

# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
COMPAT_VERSION_SHA1=$(grep 'rev =' "$NIXPKGS_COMPAT_PIN_FILE" | awk -F'"' '{ print $2 }')
COMPAT_UPSTREAM_SHA1=$(git ls-remote "$NIXPKGS_REPO.git" nixpkgs-26.05-darwin | awk '{print $1}')
update_pkgs_file "$NIXPKGS_COMPAT_PIN_FILE" "$COMPAT_VERSION_SHA1" "$COMPAT_UPSTREAM_SHA1"

nix-instantiate -I "nixpkgs=$NIXPKGS_PIN_FILE" --eval --strict --json -E "
  let
    pkgs = import <nixpkgs> {};
    opensslAttrs = builtins.filter
      (n: builtins.match \"openssl_[0-9]+(_[0-9]+)?\" n != null)
      (builtins.attrNames pkgs);
    extraMatrixAttrs = [ \"boringssl\" ];
    attrs = builtins.filter
      (n:
        let t = builtins.tryEval pkgs.\${n}; in
        t.success && (builtins.tryEval t.value.version).success
      )
      (opensslAttrs ++ extraMatrixAttrs);
  in
  {
    inherit attrs;
    permittedInsecurePackages = builtins.map (attr: pkgs.\${attr}.name) (
      builtins.filter (attr: (pkgs.\${attr}.meta.insecure)) attrs
    );
  }
" | jq -r '"{
  pkgs ? import ./pkgs.nix {
    config.permittedInsecurePackages = [ \(.permittedInsecurePackages | map(@json) | join(" ")) ];
  },
}:

{
  inherit (pkgs)
    \(.attrs | sort | join("\n    "))
    ;
}"' > "$OPENSSL_MATRIX_FILE"

cat -<<EOF
All done!

Please git add and commit the new version:

$ git add $NIXPKGS_PIN_FILE $OPENSSL_MATRIX_FILE
$ git commit -m 'tools: bump nixpkgs-unstable pin to $NEW_VERSION'
EOF

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
