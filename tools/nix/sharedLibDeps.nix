{
  pkgs ? import ./pkgs.nix { },
  withLief ? true,
  withQuic ? false,
  withSQLite ? true,
  withSSL ? true,
  withFFI ? true,
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
    nghttp2
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
// (pkgs.lib.optionalAttrs withTemporal {
  inherit (pkgs) temporal_capi;
})
