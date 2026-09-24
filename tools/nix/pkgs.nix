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
// {
  perfetto = nixpkgs.callPackage (builtins.fetchurl {
    url = "https://github.com/NixOS/nixpkgs/raw/79b35bf0bda5cd110f856aa5b5b2c5ba4460dbf5/pkgs/by-name/pe/perfetto/package.nix";
    sha256 = "0dkbyj1n1cv4s4jiwxxm1k23slvky6fj41cl4238rvkvpa74y82k";
  }) { };
}
