arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "2065d53daf2c81ed7b57947e2e56682ded62723a";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0ib5k850qp6zgzzvkbkpv0iwy649hwd8ab6id5w2jq4jplnf6zr3";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
if builtins.currentSystem == "x86_64-darwin" then (import ./pkgs-26.05.nix arg) else nixpkgs
