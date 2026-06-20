arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "49a4bd0573c376468dd7996ddb6f9fa31d8c4d97";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0f2qc9jpbzgadn2s689ss5smvh1m270p3zvrjmvl16841174lzk6";
  }) arg;
in
nixpkgs
