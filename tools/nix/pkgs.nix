arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "2074cc672349f14240fb7ea90d509e0b643e9dd4";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0x2cbp7qi17ll07dlns0bq6401r6563bvkv3bqvd6lia5ai5hj0k";
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
  }
