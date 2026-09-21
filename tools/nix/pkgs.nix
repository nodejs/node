arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "79b35bf0bda5cd110f856aa5b5b2c5ba4460dbf5";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1zqqiyjznlg8sb6zx7f218vz8cd68dxxxss8h768q515isk2dr08";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
if builtins.currentSystem == "x86_64-darwin" then (import ./pkgs-26.05.nix arg) else nixpkgs
