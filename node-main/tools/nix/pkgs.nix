arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "ffcdcf99d65c61956d882df249a9be53e5902ea5";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "09ifrgxin9may5kll1g2fdxixb0k3ipzhfw90mckl17mpbrgaglp";
  }) arg;
in
nixpkgs
