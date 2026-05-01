arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "01fbdeef22b76df85ea168fbfe1bfd9e63681b30";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0b76m4i1sn0dg78ylapvbkgw9knkf6lm1lss39w6zyshgv1rbi0q";
  }) arg;
in
nixpkgs
