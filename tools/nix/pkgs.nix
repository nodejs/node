arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "6b5e5b7a6631f065bf6908986990b37d845f847f";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0vi99516bn335vdzcjmvrkff8ikj0brpmjfcfdrjnb8bfd0wlr5j";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
if builtins.currentSystem == "x86_64-darwin" then (import ./pkgs-26.05.nix arg) else nixpkgs
