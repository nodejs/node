{
  pkgs ? import ./pkgs-26.05.nix {
    config.permittedInsecurePackages = [ "openssl-1.1.1w" "openssl-3.0.22" ];
  },
}:

{
  # "default" OpenSSL release line, should be kept in sync with the bundled version:
  openssl = pkgs.openssl_3_5;

  # Other OpenSSL variants we want to test for:
  inherit (pkgs)
    boringssl
    openssl_1_1
    openssl_3
    openssl_3_6
    openssl_4_0
    ;

  openssl_fips = import ./openssl-fips.nix { };
}
