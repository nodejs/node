arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "085af13c1607f4f73ab1e7a51071faf6577a7e87";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0wwdfxvwc19qymmjshf8qzw39dqyav1kgxna2ady9l7akqzanm1i";
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
