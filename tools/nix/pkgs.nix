arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "0bb2eedc20e4739f29e8045034f2828ad7054f8f";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "008j4dj5yrzww91qvp1lp2c9d1lkwc6i2dpainf5nkdkr3rwj8vy";
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
    # OpenSSL versions we want to keep testing with that have been removed from the unstable channel.
    inherit (import ./pkgs-26.05.nix arg)
      openssl_3
      openssl_4_0
      ;

    boringssl =
      let
        inherit (nixpkgs) boringssl lib writeTextFile;
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

    # TODO: remove this override once the pin we use has picked up https://github.com/NixOS/nixpkgs/pull/575002
    uvwasi = nixpkgs.uvwasi.overrideAttrs (old: {
      patches = old.patches ++ [
        (nixpkgs.fetchpatch2 {
          url = "https://github.com/nodejs/uvwasi/commit/fe602a6911207efd8dcfc070a752bcf76813c978.patch?full_index=1";
          hash = "sha256-MyFvRuEhcqZR+Bz2pY9vani8U1iyQrDeni2kKKnJGEI=";
        })
      ];

      buildInputs = [ ];
      propagatedBuildInputs = old.buildInputs;
    });
  }
