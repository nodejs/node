arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "aff8a0b28396750446e5537a96461bc4facdb287";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "19qc9lljqzyfxqix8z3lh72x73r0z2rijfndxvs5whdnh2vgzxf5";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
if builtins.currentSystem == "x86_64-darwin" then (import ./pkgs-26.05.nix arg) else nixpkgs
