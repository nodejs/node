arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "a8d610af3f1a5fb71e23e08434d8d61a466fc942";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0q3m5y6gz2mcmmgf5bvxamjrm6wxzcckqbj4yj1yrzp7p2c9z5mz";
  }) arg;
in
nixpkgs
