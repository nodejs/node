{
  pkgs ? import ./pkgs.nix {
    config.permittedInsecurePackages = [ "openssl-3.0.22" ];
  },
}:

{
  # "default" OpenSSL release line, should be kept in sync with the bundled version:
  openssl = pkgs.openssl_4_1;

  # Other OpenSSL variants we want to test for:
  inherit (pkgs)
    boringssl
    openssl_3
    openssl_3_5
    ;

  openssl_fips = import ./openssl-fips.nix { };
}
