{
  pkgs ? import ./tools/nix/pkgs.nix { },

  # Optional build tools / config
  ccache ? pkgs.ccache,
  loadJSBuiltinsDynamically ? true, # Load `lib/**.js` from disk instead of embedding
  ninja ? pkgs.ninja,
  extraConfigFlags ? [
    "--without-npm"
    "--debug-node"
  ],

  # Build options
  icu ? pkgs.icu,
  withAmaro ? true,
  withSQLite ? true,
  withSSL ? true,
  withTemporal ? false,
  sharedLibDeps ? import ./tools/nix/sharedLibDeps.nix {
    inherit
      pkgs
      withSQLite
      withSSL
      withTemporal
      ;
  },

  # dev tools (not needed to build Node.js, useful to maintain it)
  ncu-path ? null, # Provide this if you want to use a local version of NCU
  devTools ? import ./tools/nix/devTools.nix { inherit pkgs ncu-path; },
  benchmarkTools ? import ./tools/nix/benchmarkTools.nix { inherit pkgs; },
}:

let
  useSharedICU = if builtins.isString icu then icu == "system" else icu != null;
  useSharedAda = builtins.hasAttr "ada" sharedLibDeps;
  useSharedOpenSSL = builtins.hasAttr "openssl" sharedLibDeps;

  needsRustCompiler = withTemporal && !builtins.hasAttr "temporal_capi" sharedLibDeps;
in
pkgs.mkShell {
  inherit (pkgs.nodejs_latest) nativeBuildInputs;

  buildInputs = builtins.attrValues sharedLibDeps ++ pkgs.lib.optional useSharedICU icu;

  packages =
    pkgs.lib.optional (ccache != null) ccache
    ++ devTools
    ++ benchmarkTools
    ++ pkgs.lib.optionals needsRustCompiler [
      pkgs.cargo
      pkgs.rustc
    ];

  shellHook = pkgs.lib.optionalString (ccache != null) ''
    export CC="${pkgs.lib.getExe ccache} $CC"
    export CXX="${pkgs.lib.getExe ccache} $CXX"
  '';

  BUILD_WITH = if (ninja != null) then "ninja" else "make";
  NINJA = pkgs.lib.optionalString (ninja != null) "${pkgs.lib.getExe ninja}";
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
    ++ pkgs.lib.optional (!withAmaro) "--without-amaro"
    ++ pkgs.lib.optional (!withSQLite) "--without-sqlite"
    ++ pkgs.lib.optional (!withSSL) "--without-ssl"
    ++ pkgs.lib.optional withTemporal "--v8-enable-temporal-support"
    ++ pkgs.lib.optional (ninja != null) "--ninja"
    ++ pkgs.lib.optional loadJSBuiltinsDynamically "--node-builtin-modules-path=${builtins.toString ./.}"
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
  NOSQLITE = pkgs.lib.optionalString (!withSQLite) "1";
}
