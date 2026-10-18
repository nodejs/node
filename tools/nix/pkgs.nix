arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "c6d65881c5624c9cae5ea6cedef24699b0c0a4c0";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1yf4qv3scjygdkg67nibrhbddg3154mv9cxffvykmwcrwfcrrlaq";
  }) arg;
in
nixpkgs
// {
  boringssl =
    let
      inherit (nixpkgs) callPackage lib writeTextFile;

      # Using a pinned version of BoringSSL as we do not support the old version anymore.
      boringssl = nixpkgs.boringssl.overrideAttrs (old: {
        version = "0.20261020.0";
        src = old.src.overrideAttrs {
          hash = "sha256-PKIXmxdr8DtYuz54vxxrSNy3qNYakFoGByoTTAHCB08=";
        };
        patches = [ ];
      });
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

  perfetto = nixpkgs.callPackage (nixpkgs.fetchurl {
    url = "https://github.com/NixOS/nixpkgs/raw/79b35bf0bda5cd110f856aa5b5b2c5ba4460dbf5/pkgs/by-name/pe/perfetto/package.nix";
    hash = "sha256-UyBPjrp77oyGIJQFIp3xc1M9xAy1dx4l0WSzYIP0azY=";
  }) { };

  uvwasi = nixpkgs.uvwasi.overrideAttrs (old: {
    patches = [
      (nixpkgs.fetchpatch2 {
        url = "https://github.com/nodejs/uvwasi/pull/326.patch";
        hash = "sha256-PPr62+qycCSfr1/JWXhJS6X8vW10ea2b4L4361tdwgE=";
      })
    ];

    buildInputs = [ ];
    propagatedBuildInputs = old.buildInputs;
  });
}
