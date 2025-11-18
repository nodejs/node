arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "71cf367cc2c168b0c2959835659c38f0a342f9be";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0gqzy7r8jn2f5ymy7ixminbf7yx9dxn8kgiqm1g394x0p1rd1q00";
  }) arg;
in
nixpkgs
