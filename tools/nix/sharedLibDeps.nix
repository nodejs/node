{
  pkgs ? import ./pkgs.nix { },
  withTemporal ? false,
}:
{
  inherit (pkgs)
    ada
    brotli
    c-ares
    libuv
    nghttp2
    nghttp3
    ngtcp2
    simdjson
    simdutf
    sqlite
    uvwasi
    zlib
    zstd
    ;
  http-parser = pkgs.llhttp;
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
}
// (pkgs.lib.optionalAttrs withTemporal {
  inherit (pkgs) temporal_capi;
})
