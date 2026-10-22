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
    url = "https://github.com/NixOS/nixpkgs/raw/1c17f6e5f086bf6b259e7a28f68657ed8548a25a/pkgs/by-name/bo/boringssl/package.nix";
    sha256 = "10cw2np6shjanxa2vzfxsrbyz451g1b6qan2v9c1ln75i3n5zn8a";
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
