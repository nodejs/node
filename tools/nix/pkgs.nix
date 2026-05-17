arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "b3da656039dc7a6240f27b2ef8cc6a3ef3bccae7";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1hyl221q0c2zw3m1nv8vc39dcyrvxmn4crbn13f8p2pmcmg6x2i3";
  }) arg;
in
nixpkgs
