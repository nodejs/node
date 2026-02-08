arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "ae67888ff7ef9dff69b3cf0cc0fbfbcd3a722abe";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0hbf69ki28s8ss18x4qj9kpcw5afy8qfmd3gw1k6wn2lfhq5ddrz";
  }) arg;
in
nixpkgs
