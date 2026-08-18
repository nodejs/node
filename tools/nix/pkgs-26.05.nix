arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "e0c84f9d0ad137f076dc957494f5b39885597d4f";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0mp3pbx4mznxf55mmr5nbrac1r9i38b0dyqpynxiqwcpfhz4kfrq";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this file when 26.05 is EOL (end of 2026)
nixpkgs
