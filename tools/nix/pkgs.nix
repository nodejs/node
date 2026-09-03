arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "0e7911a191ea0fb9d6e8e771d24736d3618f8b1f";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "13ry2zkl0bbh7k7l0fpkb4xjsglzr3mzshvpcw4j9fn4s66jbmrq";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
if builtins.currentSystem == "x86_64-darwin" then (import ./pkgs-26.05.nix arg) else nixpkgs
