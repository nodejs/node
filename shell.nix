{
  pkgs ? import "${./tools/nix/pkgs.nix}" { },
  loadJSBuiltinsDynamically ? true, # Load `lib/**.js` from disk instead of embedding
  ncu-path ? null, # Provide this if you want to use a local version of NCU
  icu ? pkgs.icu,
  sharedLibDeps ? {
    inherit (pkgs)
      ada
      brotli
      c-ares
      libuv
      nghttp2
      nghttp3
      ngtcp2
      openssl
      simdjson
      simdutf
      sqlite
      uvwasi
      zlib
      zstd
      ;
    http-parser = pkgs.llhttp;
  },
  ccache ? pkgs.ccache,
  ninja ? pkgs.ninja,
  devTools ? [
    pkgs.curl
    pkgs.gh
    pkgs.git
    pkgs.jq
    pkgs.shellcheck
  ]
  ++ (
    if (ncu-path == null) then
      [ pkgs.node-core-utils ]
    else
      [
        (pkgs.writeShellScriptBin "git-node" "exec \"${ncu-path}/bin/git-node.js\" \"$@\"")
        (pkgs.writeShellScriptBin "ncu-ci" "exec \"${ncu-path}/bin/ncu-ci.js\" \"$@\"")
        (pkgs.writeShellScriptBin "ncu-config" "exec \"${ncu-path}/bin/ncu-config.js\" \"$@\"")
      ]
  ),
  benchmarkTools ? [
    pkgs.R
    pkgs.rPackages.ggplot2
    pkgs.rPackages.plyr
    pkgs.wrk
  ],
  extraConfigFlags ? [
    "--without-npm"
    "--debug-node"
  ],
}:

let
  useSharedICU = if builtins.isString icu then icu == "system" else icu != null;
  useSharedAda = builtins.hasAttr "ada" sharedLibDeps;
  useSharedOpenSSL = builtins.hasAttr "openssl" sharedLibDeps;
in
pkgs.mkShell {
  inherit (pkgs.nodejs_latest) nativeBuildInputs;

  buildInputs = builtins.attrValues sharedLibDeps ++ pkgs.lib.optionals useSharedICU [ icu ];

  packages = [
    ccache
  ]
  ++ devTools
  ++ benchmarkTools;

  shellHook =
    if (ccache != null) then
      ''
        export CC="${pkgs.lib.getExe ccache} $CC"
        export CXX="${pkgs.lib.getExe ccache} $CXX"
      ''
    else
      "";

  BUILD_WITH = if (ninja != null) then "ninja" else "make";
  NINJA = if (ninja != null) then "${pkgs.lib.getExe ninja}" else "";
  CI_SKIP_TESTS = pkgs.lib.concatStringsSep "," (
    [ ]
    ++ pkgs.lib.optionals useSharedAda [
      # Different versions of Ada affect the WPT tests
      "test-url"
    ]
    ++ pkgs.lib.optionals useSharedOpenSSL [
      # Path to the openssl.cnf is different from the expected one
      "test-strace-openat-openssl"
    ]
  );
  CONFIG_FLAGS = builtins.toString (
    [
      (
        if icu == null then
          "--without-intl"
        else
          "--with-intl=${if useSharedICU then "system" else icu}-icu"
      )
    ]
    ++ extraConfigFlags
    ++ pkgs.lib.optionals (ninja != null) [
      "--ninja"
    ]
    ++ pkgs.lib.optionals loadJSBuiltinsDynamically [
      "--node-builtin-modules-path=${builtins.toString ./.}"
    ]
    ++ pkgs.lib.concatMap (name: [
      "--shared-${builtins.replaceStrings [ "c-ares" ] [ "cares" ] name}"
      "--shared-${builtins.replaceStrings [ "c-ares" ] [ "cares" ] name}-libpath=${
        pkgs.lib.getLib sharedLibDeps.${name}
      }/lib"
      "--shared-${builtins.replaceStrings [ "c-ares" ] [ "cares" ] name}-include=${
        pkgs.lib.getInclude sharedLibDeps.${name}
      }/include"
    ]) (builtins.attrNames sharedLibDeps)
  );
}
