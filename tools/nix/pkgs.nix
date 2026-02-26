arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "d1c15b7d5806069da59e819999d70e1cec0760bf";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "08f0iv9rn4d9ha35kblqpkrgbbnfby87bj8fx1839l3r4grqdnvg";
  }) arg;
in
nixpkgs
