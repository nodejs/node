arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "fad15a0c9ebf6432dcba932743decbf8905cb024";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0q845gkz18nz463cwmkhvficl3jpncdvvnx8xlrj5gdp4c285rcn";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this file when 26.05 is EOL (end of 2026)
nixpkgs
