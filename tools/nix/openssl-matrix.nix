{
  pkgs ? import ./pkgs.nix {
    config.permittedInsecurePackages = [ "openssl-1.1.1w" ];
  },
}:

let
  boringssl = pkgs.boringssl.overrideAttrs (_: rec {
    version = "0.20260803.0";
    src = pkgs.fetchgit {
      url = "https://boringssl.googlesource.com/boringssl";
      tag = version;
      hash = "sha256-GmaXG6I2euA+Q7naO2Oxu+P4mK37RbgwW5iM7ync6Gg=";
    };
    patches = [ ];
  });
in
{
  # "default" OpenSSL release line, should be kept in sync with the bundled version:
  openssl = pkgs.openssl_3_5;

  # Other OpenSSL variants we want to test for:
  inherit boringssl;
  inherit (pkgs)
    openssl_1_1
    openssl_3
    openssl_3_6
    openssl_4_0
    ;

  openssl_fips = import ./openssl-fips.nix { };
}
