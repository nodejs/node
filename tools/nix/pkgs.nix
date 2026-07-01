arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "89570f24e97e614aa34aa9ab1c927b6578a43775";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0jfrm4wdjfg8d45b4gnxrcwa8kzclv9qisbv68v19d6fd4mdgk0h";
  }) arg;
in
nixpkgs
