arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "3d8f0f3f72a6cd4d93d0ad13203f2ea1cb7e1456";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1vq4c8nfn16zcz9sa3rjy4bhabdmnwy4pq3pdj20gzmgd3iwbrxb";
  }) arg;
in
nixpkgs
