arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "b378e416ccd4a9d27aba331c8f51a7c985dea1a0";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0n62hvmnxw6x1sxs29ryl1k4cyy0c3az9wbwpnc0v56n90x8i2v9";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this file when 26.05 is EOL (end of 2026)
nixpkgs
