arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "be5afa0fcb31f0a96bf9ecba05a516c66fcd8114";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0jm942f32ih264hmna7rhjn8964sid0sn53jwrpc73s2vyvqs7kc";
  }) arg;
in
nixpkgs
