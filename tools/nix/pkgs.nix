arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "f4b794aaffc0e3d45da81088d492b118224bb476";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0vqwmj02vmvw40kvdlf71vn5as7f4kfwrrcw778nbg3k6a1y5q8k";
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
    # OpenSSL 3.0.x was removed from unstable, but we want to keep testing it.
    inherit (import ./pkgs-26.05.nix arg)
      openssl_3
      openssl_3_6
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
  }
