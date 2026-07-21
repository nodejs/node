arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "fc51889f81924f15fba77a3c0b79cfb3f78fe0d4";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "16397a8zmfj9gygm0yj4fhp9sj6hw954k246jyzsi0xpgwdzr76h";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this file when 26.05 is EOL (end of 2026)
nixpkgs
