{
  pkgs ? import ./pkgs.nix { },
  withLief ? true,
  withQuic ? false,
  withSQLite ? true,
  withSSL ? true,
  withTemporal ? false,
  opensslVersion ? "3.5",
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
// (pkgs.lib.optionalAttrs (withSSL && opensslVersion != null) (
  let
    opensslVersions = {
      "1.1" = {
        base = pkgs.openssl_1_1;
        version = "1.1.1w";
        hash = "sha256-zzCYlQy02FOtlcCEHx+cbT3BAtzPys1SHZOSUgi3asg=";
        configureFlags = [ "no-tests" ];
      };
      "3.0" = {
        base = pkgs.openssl_3;
        version = "3.0.19";
        hash = "sha256-+lpBQ7iq4YvlPvLzyvKaLgdHQwuLx00y2IM1uUq2MHI=";
        configureFlags = [ "no-tests" ];
      };
      "3.5" = {
        base = pkgs.openssl_3_5;
        version = "3.5.5";
        hash = "sha256-soyRUyqLZaH5g7TCi3SIF05KAQCOKc6Oab14nyi8Kok=";
        configureFlags = [
          "no-docs"
          "no-tests"
        ];
      };
      "3.6" = {
        base = pkgs.openssl_3_6;
        version = "3.6.1";
        hash = "sha256-sb/tzVson/Iq7ofJ1gD1FXZ+v0X3cWjLbWTyMfUYqC4=";
        configureFlags = [
          "no-docs"
          "no-tests"
        ];
      };
    };
    selected =
      opensslVersions.${opensslVersion}
        or (throw "Unsupported opensslVersion: ${opensslVersion}. Use \"1.1\", \"3.0\", \"3.5\", \"3.6\", or null for bundled.");
  in
  {
    openssl = selected.base.overrideAttrs (old: {
      version = selected.version;
      src = pkgs.fetchurl {
        url = builtins.replaceStrings [ old.version ] [ selected.version ] old.src.url;
        hash = selected.hash;
      };
      doCheck = false;
      configureFlags = (old.configureFlags or [ ]) ++ selected.configureFlags;
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
