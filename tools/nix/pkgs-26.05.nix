arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "6062ba1a1b8b6281b12533c9075a2dbeb38f7e49";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1dfmhgmairiq0k50vflzqfqaiz6v9dfvbf60yh6fhnkna312daiy";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this file when 26.05 is EOL (end of 2026)
nixpkgs
