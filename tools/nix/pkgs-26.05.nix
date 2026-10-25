arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "752cbac9446602fbb78bdd1c3597455a3dd38432";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0h4w0f8nkmpmmpjky3blvc2r147s515pc0z4hisywln4ryryr29z";
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
