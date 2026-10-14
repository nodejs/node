arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "8edc0c72e3a38faf5434e40d1f19431125fe4b30";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0cwxl7z1x0ad9z8lcljl390y9lk73ql4fvx5lysy4s9c6723p8xn";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
if builtins.currentSystem == "x86_64-darwin" then
  (import ./pkgs-26.05.nix arg)
else
  nixpkgs
  // {
    # TODO: remove pin once https://github.com/mozilla/sccache/issues/2869 is resolved
    sccache = nixpkgs.callPackage (builtins.fetchurl {
      url = "${repo}/raw/aff8a0b28396750446e5537a96461bc4facdb287/pkgs/by-name/sc/sccache/package.nix";
      sha256 = "09dlc99sam9ld9jvzgcc00r4v4ls1cmbb0yim3mr895fpbs8b3qa";
    }) { };
  }
