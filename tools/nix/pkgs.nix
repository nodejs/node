arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "3cbe716e2346710d6e1f7c559363d14e11c32a43";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "08qnqhkcvpk2zhfsnf4k3ipcqbncmq1iv3qpfjwzrkq0l0mvky17";
  }) arg;
in
nixpkgs
