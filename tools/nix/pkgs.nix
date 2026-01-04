arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "16c7794d0a28b5a37904d55bcca36003b9109aaa";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1931vmgdclk332ikh7psxha1bskvrjwqrqm8a3xwcsr5hc8jfmbw";
  }) arg;
in
nixpkgs
