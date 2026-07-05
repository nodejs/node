arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "d33369954a67ae3322177dc9a3d564092912120c";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0ffi2k8hllbgqw9vvdfaxw88qz53gl7myll4lwri6ynq5l0lnvbc";
  }) arg;
in
nixpkgs
