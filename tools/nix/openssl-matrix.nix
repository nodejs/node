{
  pkgs ? import ./pkgs.nix {
    config.permittedInsecurePackages = [ ];
  },
}:

{
  # "default" OpenSSL release line, should be kept in sync with the bundled version:
  openssl = pkgs.openssl_3_5;

  # Other OpenSSL variants we want to test for:
  inherit (pkgs)
    boringssl
    openssl_3
    openssl_3_6
    openssl_4_0
    ;

  openssl_fips = import ./openssl-fips.nix { };
}
