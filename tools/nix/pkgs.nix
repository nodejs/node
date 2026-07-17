arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "d33369954a67ae3322177dc9a3d564092912120c";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0ffi2k8hllbgqw9vvdfaxw88qz53gl7myll4lwri6ynq5l0lnvbc";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
if builtins.currentSystem == "x86_64-darwin" then (import ./pkgs-26.05.nix arg) else nixpkgs
