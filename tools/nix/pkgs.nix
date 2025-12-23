arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "7d853e518814cca2a657b72eeba67ae20ebf7059";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0hsqizh6kqp27gywdl33rpjclz8lls8s757qwa5qkbjb9an0dxlp";
  }) arg;
in
nixpkgs
