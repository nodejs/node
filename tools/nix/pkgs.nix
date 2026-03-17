arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "f82ce7af0b79ac154b12e27ed800aeb97413723c";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0dkkyyk3y8g1a7fs4rv3lqrsmxf60vrk3q93wl7yl6ggjgds79id";
  }) arg;
in
nixpkgs
