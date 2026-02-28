# Derivation for Node.js CI (not officially supported for regular applications)
{
  pkgs ? import ./pkgs.nix { },
  buildInputs ? [ ],
  configureFlags ? [ ],
  needsRustCompiler ? false,
}:

let
  nodejs = pkgs.nodejs-slim_latest;

  version =
    let
      v8Version = builtins.match (
        ".*#define V8_MAJOR_VERSION ([0-9]+).*"
        + "#define V8_MINOR_VERSION ([0-9]+).*"
        + "#define V8_BUILD_NUMBER ([0-9]+).*"
        + "#define V8_PATCH_LEVEL ([0-9]+).*"
      ) (builtins.readFile ../../deps/v8/include/v8-version.h);
      v8_embedder_string = builtins.match ".*'v8_embedder_string': '-(node.[0-9]+)'.*" (
        builtins.readFile ../../common.gypi
      );
    in
    if v8Version == null || v8_embedder_string == null then
      throw "V8 version not found"
    else
      "${builtins.elemAt v8Version 0}.${builtins.elemAt v8Version 1}.${builtins.elemAt v8Version 2}.${builtins.elemAt v8Version 3}-${builtins.elemAt v8_embedder_string 0}";
in
pkgs.stdenv.mkDerivation (finalAttrs: {
  pname = "v8";
  inherit version;
  src =
    let
      inherit (pkgs.lib) fileset;
    in
    fileset.toSource {
      root = ../../.;
      fileset = fileset.unions [
        ../../common.gypi
        ../../configure
        ../../configure.py
        ../../deps/inspector_protocol/inspector_protocol.gyp
        ../../deps/ncrypto/ncrypto.gyp
        ../../deps/v8
        ../../node.gyp
        ../../node.gypi
        ../../src/inspector/node_inspector.gypi
        ../../src/node_version.h
        ../../tools/configure.d
        ../../tools/getmoduleversion.py
        ../../tools/getnapibuildversion.py
        ../../tools/gyp
        ../../tools/gyp_node.py
        ../../tools/icu/icu_versions.json
        ../../tools/icu/icu-system.gyp
        ../../tools/utils.py
        ../../tools/v8_gypfiles
      ];
    };

  # We need to patch tools/gyp/ to work from within Nix sandbox
  prePatch = ''
    patches=()
    for patch in ${pkgs.lib.concatStringsSep " " nodejs.patches}; do
      filtered=$(mktemp)
      filterdiff -p1 -i 'tools/gyp/*' "$patch" > "$filtered"
      if [ -s "$filtered" ]; then
        patches+=("$filtered")
      fi
    done
  '';

  inherit (nodejs) configureScript;
  inherit configureFlags buildInputs;

  nativeBuildInputs =
    nodejs.nativeBuildInputs
    ++ [
      pkgs.patchutils
      pkgs.validatePkgConfig
    ]
    ++ pkgs.lib.optionals needsRustCompiler [
      pkgs.cargo
      pkgs.rustc
    ];

  buildPhase = ''
    ninja -v -C out/Release v8_snapshot v8_libplatform
  '';
  installPhase = ''
    ${
      if pkgs.stdenv.buildPlatform.isDarwin then
        # Darwin is excluded from creating thin archive in tools/gyp/pylib/gyp/generator/ninja.py:2488
        "install -Dm644 out/Release/lib* -t $out/lib"
      else
        # On other platforms, we need to create non-thin archive.
        ''
          mkdir -p $out/lib
          for a in out/Release/obj/tools/v8_gypfiles/lib*; do
            base=$(basename "$a")
            dir=$(dirname "$a")

            (
              cd "$dir"
              "$AR" rc "$out/lib/$base" $("$AR" t "$base")
            )

            "$RANLIB" "$out/lib/$base"
          done
        ''
    }
    libs=$(for f in $out/lib/lib*.a; do
      b=$(basename "$f" .a)
      printf " -l%s" "''${b#lib}"
    done)

    # copy v8 headers
    cp -r deps/v8/include $out/

    # create a pkgconfig file for v8
    mkdir -p $out/lib/pkgconfig
    cat -> $out/lib/pkgconfig/v8.pc << EOF
    Name: v8
    Description: V8 JavaScript Engine build for Node.js CI
    Version: ${version}
    Libs: -L$out/lib $libs
    Cflags: -I$out/include
    EOF
  '';
})
