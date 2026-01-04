arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "3edc4a30ed3903fdf6f90c837f961fa6b49582d1";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0ajsdd3m4qz8mpimfcrawx81cqj8s5ypnkrxpwy7icj9j8gcpksa";
  }) arg;
in
nixpkgs
