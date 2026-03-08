arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "e38213b91d3786389a446dfce4ff5a8aaf6012f2";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0nrbkm95nl6y96l0f95hraapp7zx10c01b8m95zj8056z57dlv65";
  }) arg;
in
nixpkgs
