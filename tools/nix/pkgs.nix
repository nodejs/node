arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "391b592eb44808b3bd0cb80bb71b63a5a118b8bb";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0sf8gv1r89ahw358swal14pjk5b7xvx0p34bnqdv0h0ji53wgyss";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
if builtins.currentSystem == "x86_64-darwin" then (import ./pkgs-26.05.nix arg) else nixpkgs
