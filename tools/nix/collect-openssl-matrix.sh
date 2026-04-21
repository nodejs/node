#!/bin/sh
#
# Emits the JSON source data of OpenSSL releases to test Node.js against with
# shared libraries.
#
# This helper is used by tools/dep_updaters/update-nixpkgs-pin.sh to
# regenerate tools/nix/openssl-matrix.json.
#
# Inputs (env):
#   SUPPORTED_OPENSSL_VERSION  Latest OpenSSL major.minor cycle we support
#                              running tests with. Newer cycles are emitted
#                              with "continue-on-error": true.
#
# Output (stdout): a JSON array with shape
#   [{ "version": "3.6.1", "attr": "openssl_3_6", "continue-on-error": false }, ...]
#
# Usage: SUPPORTED_OPENSSL_VERSION=4.0 ./tools/nix/collect-openssl-matrix.sh

set -eu

: "${SUPPORTED_OPENSSL_VERSION:?SUPPORTED_OPENSSL_VERSION must be set}"

here=$(cd -- "$(dirname -- "$0")" && pwd)

# 1. Enumerate every `openssl_N` / `openssl_N_M` attribute exposed by the
#    repo-pinned nixpkgs. `tryEval` skips aliases that raise (e.g.
#    `openssl_3_0` → renamed to `openssl_3`) so we only keep attributes
#    that resolve to a real derivation with a `.version`.
nix_json=$(nix-instantiate --eval --strict --json -E "
  let
    pkgs = import $here/pkgs.nix {};
    names = builtins.filter
      (n: builtins.match \"openssl_[0-9]+(_[0-9]+)?\" n != null)
      (builtins.attrNames pkgs);
    safe = builtins.filter (n:
      let t = builtins.tryEval pkgs.\${n}; in
      t.success && (builtins.tryEval t.value.version).success) names;
  in map (n: { attr = n; version = pkgs.\${n}.version; }) safe
")

# 2. Resolve the OpenSSL version the `build` job already covers (the default
#    from sharedLibDeps.nix) so we can drop it from the matrix to avoid
#    duplicate coverage.
default_openssl_version=$(nix-instantiate --eval --strict --json -E "
  (import $here/sharedLibDeps.nix {}).openssl.version
" | jq -r .)

# 3. Fetch OpenSSL release versions from endoflife.date, keep entries that
#    are either not past EOL or still under extended support, then pick the
#    first nix attr whose `.version` starts with the release version
#    followed by `.` / letter / end-of-string (so "3.6" matches "3.6.1",
#    "1.1.1" matches "1.1.1w", and "1.1" does NOT swallow "1.1.1").
#    Releases without a matching nix attr and the one covered by default in
#    `build` are dropped.
curl -sf https://endoflife.date/api/openssl.json \
  | jq -c \
      --argjson nix "$nix_json" \
      --arg supported "$SUPPORTED_OPENSSL_VERSION" \
      --arg default_version "$default_openssl_version" '
    (now | strftime("%Y-%m-%d")) as $today |
    # Compare OpenSSL major.minor cycles as numeric tuples.
    def cycle_tuple($v):
      ($v | split(".") | map(tonumber));
    [ .[]
      | select(.eol == false or .eol > $today or .extendedSupport == true)
      | .cycle as $v
      | ($nix
          | map(select(.version | test("^" + ($v | gsub("\\."; "\\.")) + "([.a-z]|$)")))
          | first) as $m
      | select($m != null)
      | select($m.version != $default_version)
      | {
          version: $m.version,
          attr: $m.attr,
          "continue-on-error": (cycle_tuple($v) > cycle_tuple($supported))
        }
    ]'
