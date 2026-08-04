arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "329c3d2af6d1b618705150ea39f72c15eb4e613e";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0hkr1j8mm50gpxd55y85vq85bxxww8rhwf6mkvkrg0qw21dmvlmd";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this file when 26.05 is EOL (end of 2026)
nixpkgs
