{
  pkgs ? import ./pkgs.nix { },
  withLief ? true,
  withQuic ? false,
  withSQLite ? true,
  withSSL ? true,
  withTemporal ? false,
}:
{
  inherit (pkgs)
    ada
    brotli
    gtest
    libuv
    nghttp2
    simdjson
    simdutf
    uvwasi
    zlib
    zstd
    ;
  cares = pkgs.c-ares;
  hdr-histogram = pkgs.hdrhistogram_c;
  http-parser = pkgs.llhttp;
  nbytes = pkgs.callPackage "${
    pkgs.fetchgit {
      url = "https://github.com/NixOS/nixpkgs.git";
      rev = "3146c6aa9995e7351a398e17470e15305e6e18ff";
      sparseCheckout = [
        "/pkgs/by-name/nb/nbytes/"
      ];
      hash = "sha256-8cbu4ftn5ke7vd4cniwxuyKl6FRxwdToBj77oyYmsfk=";
    }
  }/pkgs/by-name/nb/nbytes/package.nix" { };
}
// (pkgs.lib.optionalAttrs withLief {
  lief = pkgs.callPackage (builtins.fetchurl {
    url = "https://github.com/NixOS/nixpkgs/raw/8368442cb2c52e6b7badf0467b454c461ffc981f/pkgs/by-name/li/lief/package.nix";
    sha256 = "0isxv5rw7m1x10k06rgwimmcbl6a1w18v8k6dqp60jr4i66lac08";
  }) { };
})
// (pkgs.lib.optionalAttrs withQuic {
  inherit (pkgs)
    nghttp3
    ngtcp2
    ;
})
// (pkgs.lib.optionalAttrs withSQLite {
  inherit (pkgs) sqlite;
})
// (pkgs.lib.optionalAttrs withSSL {
  openssl = pkgs.openssl.overrideAttrs (old: {
    version = "3.5.4";
    src = pkgs.fetchurl {
      url = builtins.replaceStrings [ old.version ] [ "3.5.4" ] old.src.url;
      hash = "sha256-lnMR+ElVMWlpvbHY1LmDcY70IzhjnGIexMNP3e81Xpk=";
    };
    doCheck = false;
    configureFlags = (old.configureFlags or [ ]) ++ [
      "no-docs"
      "no-tests"
    ];
    outputs = [
      "bin"
      "out"
      "dev"
    ];
  });
})
// (pkgs.lib.optionalAttrs withTemporal {
  inherit (pkgs) temporal_capi;
})
