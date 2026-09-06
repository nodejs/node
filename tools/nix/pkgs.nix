arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "e5ead30d0824debba629dcf0720abeddee57b7d6";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0sydiakqjp2qhr8h573bv48ng8928rjpwkx600xmxvsyz8kwczn1";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
if builtins.currentSystem == "x86_64-darwin" then (import ./pkgs-26.05.nix arg) else nixpkgs
