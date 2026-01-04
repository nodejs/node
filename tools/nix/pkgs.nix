arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "3146c6aa9995e7351a398e17470e15305e6e18ff";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "165ql727hrcjz3zwsah18zclf5znfry7dc042qxc2zixsqqzah7a";
  }) arg;
in
nixpkgs
