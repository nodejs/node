{
  pkgs ? import ./pkgs.nix { },
  withLief ? true,
  withQuic ? false,
  withSQLite ? true,
  withSSL ? true,
  withFFI ? true,
  withPerfetto ? false,
  withTemporal ? true,
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
    zlib
    zstd
    ;
  abseil = pkgs.abseil-cpp;
  cares = pkgs.c-ares;
  hdr-histogram = pkgs.hdrhistogram_c;
  highway = pkgs.libhwy;
  http-parser = pkgs.llhttp;
  uvwasi = pkgs.uvwasi.overrideAttrs (old: {
    patches = old.patches ++ [
      (pkgs.fetchpatch2 {
        url = "https://github.com/nodejs/uvwasi/pull/326.patch";
        hash = "sha256-PPr62+qycCSfr1/JWXhJS6X8vW10ea2b4L4361tdwgE=";
      })
    ];

    buildInputs = [];
    propagatedBuildInputs = old.buildInputs;
  });
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
  perfetto = pkgs.perfetto.sdk;
})
// (pkgs.lib.optionalAttrs withTemporal {
  inherit (pkgs) temporal_capi;
})
