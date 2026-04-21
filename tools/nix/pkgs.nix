arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "ab72be9733b41190ea34f1422a3e4e243ede7533";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1a720dxki2ymwiwdjn8awgpinigz5wsnxg2mmpy1s2b2wyy3gmz1";
  }) arg;
in
nixpkgs
