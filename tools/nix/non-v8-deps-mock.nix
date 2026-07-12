{
  symlinkJoin,
  writeTextFile,
  validatePkgConfig,
  testers,
  lib,
}:

let
  sharedLibsToMock = {
    zlib = [ "zlib" ];
    http-parser = [ "libllhttp" ];
    libuv = [ "libuv" ];
    ada = [ "ada" ];
    simdjson = [ "simdjson" ];
    brotli = [
      "libbrotlidec"
      "libbrotlienc"
    ];
    cares = [ "libcares" ];
    gtest = [ "gtest" ];
    hdr-histogram = [ "hdr_histogram" ];
    merve = [ "merve" ];
    nbytes = [ "nbytes" ];
    nghttp2 = [ "libnghttp2" ];
    nghttp3 = [ "libnghttp3" ];
    ngtcp2 = [ "libngtcp2" ];
    uvwasi = [ "uvwasi" ];
    zstd = [ "libzstd" ];
  };
in
symlinkJoin (finalAttrs: {
  pname = "non-v8-deps-mock";
  version = "0.0.0-mock";

  nativeBuildInputs = [ validatePkgConfig ];

  paths = lib.concatMap (
    sharedLibName:
    (builtins.map (
      pkgName:
      writeTextFile {
        name = "mock-${pkgName}.pc";
        destination = "/lib/pkgconfig/${pkgName}.pc";
        text = ''
          Name: ${pkgName}
          Description: Mock package for ${sharedLibName}
          Version: ${finalAttrs.version}
          Libs:
          Cflags:
        '';
      }
    ) sharedLibsToMock.${sharedLibName})
  ) (builtins.attrNames sharedLibsToMock);
  passthru = {
    configureFlags = [
      "--without-lief"
      "--without-sqlite"
      "--without-ffi"
      "--without-ssl"
    ]
    ++ (lib.concatMap (sharedLibName: [
      "--shared-${sharedLibName}"
      "--shared-${sharedLibName}-libname="
    ]) (builtins.attrNames sharedLibsToMock));

    tests.pkg-config = testers.hasPkgConfigModules {
      package = finalAttrs.finalPackage;
    };
  };

  meta = {
    description = "Mock of Node.js dependencies that are not needed for building V8";
    license = lib.licenses.mit;
    pkgConfigModules = lib.concatMap (x: x) (builtins.attrValues sharedLibsToMock);
  };
})
