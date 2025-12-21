arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "f997fa0f94fb1ce55bccb97f60d41412ae8fde4c";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "16x3n08n0ffqn8m2h73vk2wr0ssa8lsfvq2yc1gfd59gsss47n3m";
  }) arg;
in
nixpkgs
