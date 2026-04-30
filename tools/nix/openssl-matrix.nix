{
  pkgs ? import ./pkgs.nix {
    config.permittedInsecurePackages = [ "openssl-1.1.1w" ];
  },
}:

{
  inherit (pkgs)
    openssl_1_1
    openssl_3
    openssl_3_5
    openssl_3_6
    openssl_4_0
    ;
}
