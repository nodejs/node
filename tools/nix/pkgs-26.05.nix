arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "c19db427a1fdfc7591c0b0baeb4665dcef2c61da";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0chqd1d3hbqwnz7arg5lh6iqv13n13zlhia52wxn72h3r5mi5jcy";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this file when 26.05 is EOL (end of 2026)
nixpkgs
