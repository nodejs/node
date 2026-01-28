arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "ab9fbbcf4858bd6d40ba2bbec37ceb4ab6e1f562";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1jkl1sjn89s8sd7x99kv0j2khhbz5vzpyf3iiscq3r0ybnjlj1wq";
  }) arg;
in
nixpkgs
