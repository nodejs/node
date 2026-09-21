arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "0c32f40fe3e2a9adfc427fd5abc061a31043ea44";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "11gv5qs8r9vljy5ja4figvfpflffip0ylnnqdik04054ni6crh2n";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this file when 26.05 is EOL (end of 2026)
nixpkgs
