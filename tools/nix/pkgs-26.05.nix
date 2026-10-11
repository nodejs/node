arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "240fddad86d7a0520319f21d8be1b43db426b18d";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0ycjk076iak360rxmgpma4wajqdffncidgrfa11kvdhplaxr4zn8";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this file when 26.05 is EOL (end of 2026)
nixpkgs
// {
  simdutf = nixpkgs.simdutf.overrideAttrs (old: {
    # TODO: remove this once the pin we use has picked up https://github.com/NixOS/nixpkgs/pull/557405
    cmakeFlags = old.cmakeFlags ++ [ (nixpkgs.lib.cmakeFeature "SIMDUTF_CXX_STANDARD" "20") ];
  });
}
