arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "572a2c2b6faebd71246e3162e4217d7ca63a9300";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0c5xyqgip1kf7hinqbmfvsf8c7jwipbyj7dlb337gd058kz7zmwm";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this file when 26.05 is EOL (end of 2026)
nixpkgs
