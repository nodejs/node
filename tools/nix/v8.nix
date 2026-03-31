# Derivation for Node.js CI (not officially supported for regular applications)
{
  stdenv,
  lib,
  patchutils,
  validatePkgConfig,
  nodejs-slim_latest,
  icu,

  buildInputs ? lib.optional (icu != null) icu,
  configureFlags ? [
    (
      if icu == null then
        "--without-intl"
      else
        "--with-intl=${if builtins.isString icu then icu else "system"}-icu"
    )
  ],

  configureScript ? nodejs-slim_latest.configureScript,
  nativeBuildInputs ? nodejs-slim_latest.nativeBuildInputs,
  patches ? nodejs-slim_latest.patches,
}:

let
  v8Dir = ../../deps/v8;
in
stdenv.mkDerivation (finalAttrs: {
  pname = "v8";
  version =
    let
      v8Version = builtins.match (
        ".*#define V8_MAJOR_VERSION ([0-9]+).*"
        + "#define V8_MINOR_VERSION ([0-9]+).*"
        + "#define V8_BUILD_NUMBER ([0-9]+).*"
        + "#define V8_PATCH_LEVEL ([0-9]+).*"
      ) (builtins.readFile "${v8Dir}/include/v8-version.h");
      v8_embedder_string = builtins.match ".*'v8_embedder_string': '-(node.[0-9]+)'.*" (
        builtins.readFile ../../common.gypi
      );
    in
    if v8Version == null || v8_embedder_string == null then
      throw "V8 version not found"
    else
      "${builtins.elemAt v8Version 0}.${builtins.elemAt v8Version 1}.${builtins.elemAt v8Version 2}.${builtins.elemAt v8Version 3}-${builtins.elemAt v8_embedder_string 0}";
  src =
    let
      inherit (lib) fileset;
    in
    fileset.toSource {
      root = ../../.;
      fileset = fileset.unions (
        [
          v8Dir
          ../../common.gypi
          ../../configure.py
          ../../node.gyp
          ../../node.gypi
          ../../src/node_version.h
          ../../tools/configure.d/nodedownload.py
          ../../tools/getmoduleversion.py
          ../../tools/getnapibuildversion.py
          ../../tools/gyp_node.py
          ../../tools/utils.py
          ../../tools/v8_gypfiles/abseil.gyp
          ../../tools/v8_gypfiles/features.gypi
          ../../tools/v8_gypfiles/ForEachFormat.py
          ../../tools/v8_gypfiles/ForEachReplace.py
          ../../tools/v8_gypfiles/GN-scraper.py
          ../../tools/v8_gypfiles/inspector.gypi
          ../../tools/v8_gypfiles/toolchain.gypi
          ../../tools/v8_gypfiles/v8.gyp
        ]
        ++ lib.optionals (icu != null) [
          ../../tools/icu/icu_versions.json
          ../../tools/icu/icu-system.gyp
        ]
        ++ lib.optionals (icu == "small") [
          ../../deps/icu-small
          ../../tools/icu/current_ver.dep
          ../../tools/icu/icu_small.json
          ../../tools/icu/icu-generic.gyp
          ../../tools/icu/iculslocs.cc
          ../../tools/icu/icutrim.py
          ../../tools/icu/no-op.cc
        ]
      );
    };

  # We need to download and patch GYP to work from within Nix sandbox
  # and so the local pycache does not pollute the hash.
  prePatch = ''
    patches=()
    for patch in ${lib.concatStringsSep " " patches}; do
      filtered=$(mktemp)
      filterdiff -p1 -i 'tools/gyp/pylib/*' "$patch" > "$filtered"
      if [ -s "$filtered" ]; then
        patches+=("$filtered")
      fi
    done
    tar -C tools -xzf ${import ../../tools/gyp/src.nix} --wildcards 'gyp-*/pylib'
    mv tools/gyp-* tools/gyp
  '';
  # We need to remove the node_inspector.gypi ref so GYP does not search for it.
  postPatch = ''
    substituteInPlace node.gyp --replace-fail "'includes' : [ 'src/inspector/node_inspector.gypi' ]" "'includes' : []"
  ''
  + lib.optionalString (icu == null) ''
    substituteInPlace configure.py \
      --replace-fail \
        "icu_versions = json.loads((tools_path / 'icu' / 'icu_versions.json').read_text(encoding='utf-8'))" \
        "icu_versions = { 'minimum_icu': 1 }"
  '';

  inherit configureScript configureFlags buildInputs;

  nativeBuildInputs = nativeBuildInputs ++ [
    patchutils
    validatePkgConfig
  ];

  buildPhase = ''
    ninja -v -C out/Release v8_snapshot v8_libplatform
  '';
  installPhase = ''
    ${
      if stdenv.hostPlatform.isDarwin then
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

    mkdir -p $out/lib/pkgconfig
    cat -> $out/lib/pkgconfig/v8.pc << EOF
    Name: v8
    Description: V8 JavaScript Engine build for Node.js CI
    Version: ${finalAttrs.version}
    Libs: -L$out/lib $(for f in $out/lib/lib*.a; do
      b=$(basename "$f" .a)
      printf " -l%s" "''${b#lib}"
    done) -lstdc++
    Cflags: -I${v8Dir}/include -I${v8Dir}/third_party/abseil-cpp -I${v8Dir}/third_party/simdutf
    EOF
  '';
})
