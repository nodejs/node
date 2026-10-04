arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "e49322d1ec25b45f7c587c5fd69ac826d1a57dbc";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "09c75qhjxdg8ls8fi89vl1fknhmszcan5jnrni5cr6zlv911x7pg";
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
