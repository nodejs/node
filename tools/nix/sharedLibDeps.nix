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
    merve
    nbytes
    simdjson
    simdutf
    uvwasi
    zlib
    zstd
    ;
  cares = pkgs.c-ares;
  hdr-histogram = pkgs.hdrhistogram_c;
  http-parser = pkgs.llhttp;
  nghttp2 = pkgs.nghttp2.overrideAttrs {
    patches = [
      (pkgs.fetchpatch2 {
        url = "https://github.com/nghttp2/nghttp2/commit/7784fa979d0bcf801a35f1afbb25fb048d815cd7.patch?full_index=1";
        revert = true;
        excludes = [ "lib/includes/nghttp2/nghttp2.h" ];
        hash = "sha256-RG87Qifjpl7HTP9ac2JwHj2XAbDlFgOpAnpZX3ET6gU=";
      })
    ];
  };
}
// (pkgs.lib.optionalAttrs withLief {
  inherit (pkgs) lief;
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
// (pkgs.lib.optionalAttrs withSSL (
  let
    version = "3.5.5";
    opensslSrc = "/pkgs/development/libraries/openssl/";
    inherit
      (pkgs.callPackage "${
        pkgs.fetchgit {
          url = "https://github.com/NixOS/nixpkgs.git";
          rev = "a5b50d31e0fd60227495ad2b2760cbda3581ec77";
          sparseCheckout = [ opensslSrc ];
          nonConeMode = true;
          hash = "sha256-Qo3IoUeccGO2GxFSYufyYjZmN5LGSek0z82pN73YXic=";
        }
      }${opensslSrc}" { })
      openssl_3_6
      ;
  in
  {
    openssl = openssl_3_6.overrideAttrs (old: {
      inherit version;
      src = pkgs.fetchurl {
        url = builtins.replaceStrings [ old.version ] [ version ] old.src.url;
        hash = "sha256-soyRUyqLZaH5g7TCi3SIF05KAQCOKc6Oab14nyi8Kok=";
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
  }
))
// (pkgs.lib.optionalAttrs withTemporal {
  inherit (pkgs) temporal_capi;
})
