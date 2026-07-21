arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "20535e48e12c86043b577b8518234ff5dbb26957";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1dmdschkpmhjp67rhsig7k2qhgd918j5g30s6yxmjljqsxh2vlh9";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
if builtins.currentSystem == "x86_64-darwin" then (import ./pkgs-26.05.nix arg) else nixpkgs
