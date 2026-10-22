arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "f20cd90e28fe0403873f473b29e42662cd86e8c4";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1vv6crfchmhjk5j61zgajggc3cgkalak2rvng07g41cvlcf6ldhj";
  }) arg;
in
# Unstable channel no longer supports Intel architecture for macOS. We can use the 26.05 channel
# to keep testing on that platform for a little longer.
# TODO: remove this when 26.05 is EOL (end of 2026)
if builtins.currentSystem == "x86_64-darwin" then
  (import ./pkgs-26.05.nix arg)
else
  nixpkgs
  // {
    # OpenSSL 1.1.x and 3.0.x were removed from unstable, we want to keep testing with it
    inherit (import ./pkgs-26.05.nix arg)
      openssl_1_1
      openssl_3
      ;

    boringssl =
      let
        inherit (nixpkgs) callPackage lib writeTextFile;
        # TODO: remove this override once nixpkgs.boringssl is 0.20261020.0 or newer.
        boringssl = callPackage (builtins.fetchurl {
          url = "${repo}/raw/1c17f6e5f086bf6b259e7a28f68657ed8548a25a/pkgs/by-name/bo/boringssl/package.nix";
          sha256 = "10cw2np6shjanxa2vzfxsrbyz451g1b6qan2v9c1ln75i3n5zn8a";
        }) { };
      in
      # BoringSSL does not install the openssl.pc metadata required by configure.
      # https://boringssl-review.googlesource.com/c/boringssl/+/104208
      writeTextFile {
        inherit (boringssl) name;
        passthru = { inherit (boringssl) version; };
        destination = "/lib/pkgconfig/openssl.pc";
        text = ''
          Name: BoringSSL
          Description: BoringSSL cryptography library
          Version: ${boringssl.version}
          Libs: -L${lib.getLib boringssl}/lib -lcrypto -lssl
          Cflags: -I${lib.getInclude boringssl}/include
        '';
      };

    # TODO: remove pin once https://github.com/mozilla/sccache/issues/2869 is resolved
    sccache = nixpkgs.callPackage (builtins.fetchurl {
      url = "${repo}/raw/aff8a0b28396750446e5537a96461bc4facdb287/pkgs/by-name/sc/sccache/package.nix";
      sha256 = "09dlc99sam9ld9jvzgcc00r4v4ls1cmbb0yim3mr895fpbs8b3qa";
    }) { };
  }
