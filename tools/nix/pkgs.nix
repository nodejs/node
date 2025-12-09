arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "a672be65651c80d3f592a89b3945466584a22069";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1p2g7g9vlpv14xz6rrlb8i6w8sxki8i35m15phjiwfrqaf6x508i";
  }) arg;
in
nixpkgs
