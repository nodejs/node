arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "e9a7635a57597d9754eccebdfc7045e6c8600e6b";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1515byrd40ylb3z258g4f9mmhn1v608dvmxi8acci8vz4zzr99dv";
  }) arg;
in
nixpkgs
