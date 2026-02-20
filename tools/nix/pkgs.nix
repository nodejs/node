arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "2343bbb58f99267223bc2aac4fc9ea301a155a16";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "03qwpi78578vlp1y1wfg5yrfyinp82sq16z910gijpphc16dd2rf";
  }) arg;
in
nixpkgs
