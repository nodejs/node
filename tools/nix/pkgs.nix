arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "3b32825de172d0bc85664f495edb096b10862524";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "17fpx73lbg8ynijalvmk1jzz77a2jvl7063kjd2226fmajqn59nr";
  }) arg;
in
nixpkgs
