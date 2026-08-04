#!/bin/sh
set -ex
# Shell script to update Nixpkgs pin in the source tree to the most recent
# version on the unstable channel.

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
NIXPKGS_PIN_FILE="$BASE_DIR/tools/nix/pkgs.nix"
OPENSSL_MATRIX_FILE="$BASE_DIR/tools/nix/openssl-matrix.nix"

NIXPKGS_REPO=$(grep 'repo =' "$NIXPKGS_PIN_FILE" | awk -F'"' '{ print $2 }')
CURRENT_VERSION_SHA1=$(grep 'rev =' "$NIXPKGS_PIN_FILE" | awk -F'"' '{ print $2 }')

NEW_UPSTREAM_SHA1=$(git ls-remote "$NIXPKGS_REPO.git" nixpkgs-unstable | awk '{print $1}')
NEW_VERSION=$(echo "$NEW_UPSTREAM_SHA1" | head -c 35)


# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

compare_dependency_version "nixpkgs-unstable" "$CURRENT_VERSION_SHA1" "$NEW_UPSTREAM_SHA1"

CURRENT_TARBALL_HASH=$(grep 'sha256 =' "$NIXPKGS_PIN_FILE" | awk -F'"' '{ print $2 }')
NEW_TARBALL_HASH=$(nix-prefetch-url --unpack "$NIXPKGS_REPO/archive/$NEW_UPSTREAM_SHA1.tar.gz")

TMP_FILE=$(mktemp)
sed "s/$CURRENT_VERSION_SHA1/$NEW_UPSTREAM_SHA1/;s/$CURRENT_TARBALL_HASH/$NEW_TARBALL_HASH/" "$NIXPKGS_PIN_FILE" > "$TMP_FILE"
mv "$TMP_FILE" "$NIXPKGS_PIN_FILE"

# === Update openssl-matrix.nix ===
# When bumping the pin, we want to update the openssl-matrix.nix file to keep the list in sync nixpkgs
# i.e. add newly added release lines, remove newly dropped release lines), and make sure the "openssl"
# attribute still refers to the same release line as the bundled version in deps/openssl/.

OPENSSL_MAJOR=$(awk -F= '/^MAJOR=[0-9]+$/ { print $2; exit }' "$BASE_DIR/deps/openssl/openssl/VERSION.dat")
OPENSSL_MINOR=$(awk -F= '/^MINOR=[0-9]+$/ { print $2; exit }' "$BASE_DIR/deps/openssl/openssl/VERSION.dat")

nix-instantiate -I "nixpkgs=$NIXPKGS_PIN_FILE" --eval --strict --json -E "
  let
    pkgs = import <nixpkgs> {};
    opensslAttrs = builtins.filter
      (n: builtins.match \"openssl_[0-9]+(_[0-9]+)?\" n != null)
      (builtins.attrNames pkgs);
    extraMatrixAttrs = [ \"boringssl\" ];
    default = builtins.head (builtins.filter (n:
      let
        inherit (pkgs.lib) versions;
        t = builtins.tryEval pkgs.\${n};
        v = if t.success then builtins.tryEval t.value.version else t;
        majorVersion = pkgs.lib.optionalString v.success (versions.major v.value);
        minorVersion = pkgs.lib.optionalString v.success (versions.minor v.value);
      in
        majorVersion == ''$OPENSSL_MAJOR'' && minorVersion == ''$OPENSSL_MINOR'') opensslAttrs);
    attrs = builtins.filter
      (n:
        let t = builtins.tryEval pkgs.\${n}; in
        n != default && t.success && (builtins.tryEval t.value.version).success
      )
      (opensslAttrs ++ extraMatrixAttrs);
  in
  {
    inherit attrs default;
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
  # "default" OpenSSL release line, should be kept in sync with the bundled version:
  openssl = pkgs.\(.default);

  # Other OpenSSL variants we want to test for:
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
