arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "a7fc11be66bdfb5cdde611ee5ce381c183da8386";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0h3gvjbrlkvxhbxpy01n603ixv0pjy19n9kf73rdkchdvqcn70j2";
  }) arg;
in
nixpkgs
