#!/bin/sh
#
# Emits the JSON source data of OpenSSL releases to test Node.js against with
# shared libraries.
#
# This helper is used by tools/dep_updaters/update-nixpkgs-pin.sh to
# regenerate tools/nix/openssl-matrix.json.
#
# Output (stdout): a JSON array with shape
#   [{ "version": "3.6.1", "attr": "openssl_3_6", "continue-on-error": false }, ...]
#
# Usage: ./tools/nix/collect-openssl-matrix.sh

set -eu

# Latest OpenSSL major.minor cycle we support
# running tests with. Newer cycles are emitted
# with "continue-on-error": true.
SUPPORTED_OPENSSL_VERSION=4.0

here=$(cd -- "$(dirname -- "$0")" && pwd)

# 1. Enumerate every `openssl_N` / `openssl_N_M` attribute exposed by the
#    repo-pinned nixpkgs. `tryEval` skips aliases that raise (e.g.
#    `openssl_3_0` → renamed to `openssl_3`) so we only keep attributes
#    that resolve to a real derivation with a `.version`.
nix-instantiate --eval --strict --json -E "
  let
    pkgs = import $here/pkgs.nix {};
    attrs = builtins.filter
      (n:
        let t = builtins.tryEval pkgs.\${n}; in
        t.success && (builtins.tryEval t.value.version).success
      )
      (
        builtins.filter
          (n: builtins.match \"openssl_[0-9]+(_[0-9]+)?\" n != null)
          (builtins.attrNames pkgs)
      );
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
    \(.attrs | join("\n    "))
    ;
}"'
