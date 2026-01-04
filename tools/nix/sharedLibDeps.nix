{
  pkgs ? import ./pkgs.nix { },
  withSQLite ? true,
  withSSL ? true,
  withTemporal ? false,
}:
{
  inherit (pkgs)
    ada
    brotli
    c-ares
    libuv
    nghttp3
    ngtcp2
    simdjson
    simdutf
    uvwasi
    zlib
    zstd
    ;
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
