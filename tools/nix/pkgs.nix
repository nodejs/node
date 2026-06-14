arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "cbb5cf358f50aa6acc9efd6113b7bcfbc352cd73";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "005lz0rdsj188j63zyim4pkxadqcq276x7w9727ymb2av7awczi1";
  }) arg;
in
nixpkgs
