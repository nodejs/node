arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "ca77296380960cd497a765102eeb1356eb80fed0";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1airrw6l87iyny1a3mb29l28na4s4llifprlgpll2na461jd40iy";
  }) arg;
in
nixpkgs
