arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "f6107e546a5012172d93e79f1f7950da02ad798f";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "04n8l03dspn0747lc2vhkzbxbjc9wyx0ay959wdj35qnpyzxap5w";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this file when 26.05 is EOL (end of 2026)
nixpkgs
