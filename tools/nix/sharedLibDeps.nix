{
  pkgs ? import ./pkgs.nix { },
  withLief ? true,
  withQuic ? false,
  withSQLite ? true,
  withSSL ? true,
  withFFI ? true,
  withPerfetto ? false,
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
  abseil = pkgs.abseil-cpp;
  cares = pkgs.c-ares;
  hdr-histogram = pkgs.hdrhistogram_c;
  highway = pkgs.libhwy;
  http-parser = pkgs.llhttp;
  nghttp2 = pkgs.nghttp2.overrideAttrs {
    version = "1.69.0";
    src = pkgs.fetchurl {
      url = "https://github.com/nghttp2/nghttp2/releases/download/v1.69.0/nghttp2-1.69.0.tar.bz2";
      hash = "sha256-PxhfWxw+d4heuc8/LE2ksan3OiS/WVe4KRg60Tf4Lcg=";
    };
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
// (pkgs.lib.optionalAttrs withFFI {
  ffi = pkgs.libffiReal;
})
// (pkgs.lib.optionalAttrs withSSL ({
  inherit (import ./openssl-matrix.nix { inherit pkgs; }) openssl;
}))
// (pkgs.lib.optionalAttrs withPerfetto {
  perfetto =
    (pkgs.callPackage (builtins.fetchurl {
      url = "https://github.com/NixOS/nixpkgs/raw/c19db427a1fdfc7591c0b0baeb4665dcef2c61da/pkgs/by-name/pe/perfetto/package.nix";
      sha256 = "0wc4p96kwxpqr8g2a6lmlfjsffrm3dc824m7hiy0q21r2papc0ik";
    }) { }).sdk;
})
// (pkgs.lib.optionalAttrs withTemporal {
  inherit (pkgs) temporal_capi;
})
