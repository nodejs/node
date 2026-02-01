arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "6308c3b21396534d8aaeac46179c14c439a89b8a";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "14qnx22pkl9v4r0lxnnz18f4ybxj8cv18hyf1klzap98hckg58y4";
  }) arg;
in
nixpkgs
