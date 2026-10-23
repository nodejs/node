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
    # OpenSSL 3.0.x was removed from unstable, but we want to keep testing it.
    inherit (import ./pkgs-26.05.nix arg)
      openssl_3
      ;

    # TODO: remove this when the pin we use has picked up https://boringssl-review.googlesource.com/c/boringssl/+/104208
    boringssl =
      let
        inherit (nixpkgs) boringssl lib writeTextFile;
      in
      writeTextFile {
        inherit (boringssl) name;
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
