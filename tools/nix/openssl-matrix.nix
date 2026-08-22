{
  pkgs ? import ./pkgs.nix {
    config.permittedInsecurePackages = [ "openssl-1.1.1w" ];
  },
}:

{
  # "default" OpenSSL release line, should be kept in sync with the bundled version:
  openssl = pkgs.openssl_3_5;

  # Using an updated version of BoringSSL as we do not support the old version anymore.
  boringssl = pkgs.callPackage (builtins.fetchurl {
    url = "https://github.com/NixOS/nixpkgs/raw/6062ba1a1b8b6281b12533c9075a2dbeb38f7e49/pkgs/by-name/bo/boringssl/package.nix";
    sha256 = "01n2ga83ds5m9hz88z635g3q1c141ka2qs9k576c62v3fdpgcj6i";
  }) { };

  # Other OpenSSL variants we want to test for:
  inherit (pkgs)
    openssl_1_1
    openssl_3
    openssl_3_6
    openssl_4_0
    ;

  openssl_fips = import ./openssl-fips.nix { };
}
