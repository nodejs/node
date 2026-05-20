{
  pkgs ? import ./tools/nix/pkgs.nix { },

  # Optional build tools / config
  ccache ? pkgs.ccache,
  loadJSBuiltinsDynamically ? true, # Load `lib/**.js` from disk instead of embedding
  ninja ? pkgs.ninja,
  extraConfigFlags ? [
    "--debug-node"
  ],
  useSeparateDerivationForV8 ? false, # to help CI better managed its binary cache, not recommended outside of CI usage.

  # Build options
  icu ? pkgs.icu,
  withAmaro ? true,
  withLief ? true,
  withQuic ? false,
  withSQLite ? true,
  withFFI ? true,
  withSSL ? true,
  withTemporal ? false,
  sharedLibDeps ? (
    import ./tools/nix/sharedLibDeps.nix {
      inherit
        pkgs
        withLief
        withQuic
        withSQLite
        withFFI
        withSSL
        withTemporal
        ;
    }
  ),

  # dev tools (not needed to build Node.js, useful to maintain it)
  ncu-path ? null, # Provide this if you want to use a local version of NCU
  devTools ? import ./tools/nix/devTools.nix { inherit pkgs ncu-path; },
  benchmarkTools ? import ./tools/nix/benchmarkTools.nix { inherit pkgs; },
}:

let
  useSharedICU = if builtins.isString icu then icu == "system" else icu != null;
  useSharedAda = builtins.hasAttr "ada" sharedLibDeps;
  useSharedOpenSSL = builtins.hasAttr "openssl" sharedLibDeps;

  useSharedTemporal = builtins.hasAttr "temporal_capi" sharedLibDeps;
  needsRustCompiler = withTemporal && !useSharedTemporal;

  nativeBuildInputs =
    pkgs.nodejs-slim_latest.nativeBuildInputs
    ++ pkgs.lib.optionals needsRustCompiler [
      pkgs.cargo
      pkgs.rustc
    ];
  buildInputs =
    pkgs.lib.optional useSharedICU icu
    ++ pkgs.lib.optional (withTemporal && useSharedTemporal) sharedLibDeps.temporal_capi;

  # Put here only the configure flags that affect the V8 build
  configureFlags = [
    (
      if icu == null then
        "--without-intl"
      else
        "--with-intl=${if useSharedICU then "system" else icu}-icu"
    )
    "--v8-${if withTemporal then "enable" else "disable"}-temporal-support"
  ]
  ++ pkgs.lib.optional (withTemporal && useSharedTemporal) "--shared-temporal_capi";
in
pkgs.mkShell {
  inherit nativeBuildInputs;

  buildInputs =
    builtins.attrValues sharedLibDeps
    ++ buildInputs
    ++ pkgs.lib.optional (useSeparateDerivationForV8 != false) (
      if useSeparateDerivationForV8 == true then
        let
          sharedLibsToMock = pkgs.callPackage ./tools/nix/non-v8-deps-mock.nix { };
        in
        pkgs.callPackage ./tools/nix/v8.nix {
          inherit nativeBuildInputs icu;

          configureFlags = configureFlags ++ sharedLibsToMock.configureFlags ++ [ "--ninja" ];
          buildInputs = buildInputs ++ [ sharedLibsToMock ];
        }
      else
        useSeparateDerivationForV8
    );

  packages = devTools ++ benchmarkTools ++ pkgs.lib.optional (ccache != null) ccache;

  shellHook = pkgs.lib.optionalString (ccache != null) ''
    export CC="${pkgs.lib.getExe ccache} $CC"
    export CXX="${pkgs.lib.getExe ccache} $CXX"
  '';

  BUILD_WITH = if (ninja != null) then "ninja" else "make";
  NINJA = pkgs.lib.optionalString (ninja != null) "${pkgs.lib.getExe ninja}";
  CONFIG_FLAGS = builtins.toString (
    configureFlags
    ++ extraConfigFlags
    ++ pkgs.lib.optional (ninja != null) "--ninja"
    ++ pkgs.lib.optional (!withAmaro) "--without-amaro"
    ++ pkgs.lib.optional (!withLief) "--without-lief"
    ++ pkgs.lib.optional withQuic "--experimental-quic"
    ++ pkgs.lib.optional (!withSQLite) "--without-sqlite"
    ++ pkgs.lib.optional (!withFFI) "--without-ffi"
    ++ pkgs.lib.optional (!withSSL) "--without-ssl"
    ++ pkgs.lib.optional loadJSBuiltinsDynamically "--node-builtin-modules-path=${builtins.toString ./.}"
    ++ pkgs.lib.optional (useSeparateDerivationForV8 != false) "--without-bundled-v8"
    ++
      pkgs.lib.concatMap
        (name: [
          "--shared-${name}"
          "--shared-${name}-libpath=${pkgs.lib.getLib sharedLibDeps.${name}}/lib"
          "--shared-${name}-include=${pkgs.lib.getInclude sharedLibDeps.${name}}/include"
        ])
        (
          builtins.attrNames (
            if (useSeparateDerivationForV8 != false) then
              builtins.removeAttrs sharedLibDeps [
                "simdutf"
                "temporal_capi"
              ]
            else
              sharedLibDeps
          )
        )
  );
  NOSQLITE = pkgs.lib.optionalString (!withSQLite) "1";
}
