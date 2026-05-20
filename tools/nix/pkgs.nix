arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "d233902339c02a9c334e7e593de68855ad26c4cb";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1485vqhb8cwym1m75v61i10j427vazszaklkwj2wmm80k8sijjyz";
  }) arg;
in
nixpkgs
