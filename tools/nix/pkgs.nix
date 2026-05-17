arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "c6d65881c5624c9cae5ea6cedef24699b0c0a4c0";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1yf4qv3scjygdkg67nibrhbddg3154mv9cxffvykmwcrwfcrrlaq";
  }) arg;
in
nixpkgs
