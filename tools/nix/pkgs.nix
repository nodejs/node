arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "3e41b24abd260e8f71dbe2f5737d24122f972158";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1rb3aw213s8ms3nxj9b1dya90zh1drscjq7aly4v85farywvw4xg";
  }) arg;
in
nixpkgs
