arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "bcdf747749ad31ab043d6341a18699a8b9b62ef0";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "10vv47y0b3k3aq2l52rqd5qzk50a7jbdcamd8rwc0g02mm6blxn8";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
if builtins.currentSystem == "x86_64-darwin" then (import ./pkgs-26.05.nix arg) else nixpkgs
