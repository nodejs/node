arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "59b6c96beacc898566c9be1052ae806f3835f87d";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1b8j0xs1z1d13qrb2l5kmrrl5pydwf56a097jyla65f9218n52aj";
  }) arg;
in
nixpkgs
