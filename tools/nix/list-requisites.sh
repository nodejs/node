#!/bin/sh

set -ex

OPTIONAL_FLAGS=$(nix-instantiate \
  --eval --strict --json -E '
    builtins.filter
      (n: builtins.match "with[A-Z].+" n != null)
      (builtins.attrNames (builtins.functionArgs (import ./shell.nix)))
  ' | jq -r 'map("--arg \(.) true") | join(" ")')

DRV=$(
  # shellcheck disable=SC2086
  nix-instantiate -I "nixpkgs=./tools/nix/pkgs.nix" shell.nix \
    $OPTIONAL_FLAGS \
    --arg sharedLibDeps '{
      # Using an empty set as some build dependencies are required only in the absence of shared deps (e.g. Cargo).
      # We pass the shared deps as devTools below so they are still accounted for.
    }' \
    --arg pkcs11 'import ./tools/nix/pkcs11.nix {
      # Passing an import call rather than "true" to workaround sharedLibDeps being empty.
    }' \
    --arg devTools '
      let
        pkgs = import <nixpkgs> { };
        sharedLibDepsFn = import ./tools/nix/sharedLibDeps.nix;
      in
      (import ./tools/nix/devTools.nix { })
      ++ pkgs.lib.flatten (
        with (pkgs.callPackage ./tools/nix/v8.nix { });
        [
          # We do not want to build V8 here, but still want to list its requisites.
          buildInputs
          nativeBuildInputs
          propagatedBuildInputs
          propagatedNativeBuildInputs
          (pkgs.callPackage ./tools/nix/non-v8-deps-mock.nix { })
        ]
      )
      ++ builtins.attrValues (
        {
          # Additional packages we are using across the codebase
          inherit (pkgs) nixfmt-tree sccache;
        }
        // import ./tools/nix/openssl-matrix.nix { }
        // sharedLibDepsFn (
          pkgs.lib.filterAttrs (n: v: builtins.match "with[A-Z].+" n != null) (
            builtins.functionArgs sharedLibDepsFn
          )
        )
      )'
)
REFS=$(nix-store --query --references "$DRV")
STORE_PATHS=$(echo "$REFS" | xargs nix-store --realise)
REQUISITES=$(echo "$STORE_PATHS" | xargs nix-store --query --requisites)

echo "$REQUISITES" | sort -k1.45
