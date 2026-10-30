{
  pinFile ? ./pkgs.nix,
  shellFile ? ../../shell.nix,
  jsonFile ? ../perfetto/trace_processor_shell.json,
  system ? "x86_64-linux",
}:
let
  pkgs = import pinFile {
    inherit system;
    overlays = [
      (
        final: prev:
        prev.lib.optionalAttrs (prev.stdenv.hostPlatform.system == "x86_64-linux") {
          perfetto = prev.perfetto.overrideAttrs (old: {
            gnFlags = old.gnFlags ++ [
              "enable_perfetto_x64_cpu_opt=false"
            ];
          });
        }
      )
    ];
    crossOverlays = [
      (
        final: prev:
        prev.lib.optionalAttrs (prev.stdenv.hostPlatform.system == "s390x-linux") (
          let
            zlib = prev.zlib.overrideAttrs (old: {
              postPatch =
                old.postPatch
                # musl only defines HWCAP_S390_VXRS; glibc defines HWCAP_S390_VX as its alias.
                + ''
                  substituteInPlace contrib/crc32vx/crc32_vx.c \
                    --replace-fail 'HWCAP_S390_VX)' 'HWCAP_S390_VXRS)'
                '';
            });
          in
          {
            protobuf = prev.protobuf.override { inherit zlib; };
            perfetto =
              let
                perfetto = prev.perfetto.override {
                  inherit zlib;
                  sqlite = prev.sqlite.override { inherit zlib; };
                };
              in
              perfetto.overrideAttrs (old: {
                src = final.fetchFromGitHub {
                  owner = "iii-i";
                  repo = "perfetto_protos";
                  tag = "iii/s390x/2026-10-27";
                  hash = "sha256-hQvCrko3/lRVAy51TvaZLtKn23dCTvr96uHC5qUEkJE=";
                };

                ninjaFlags = [ "trace_processor_shell" ];
                installPhase = "install -Dt $tools/bin $ninjaFlags";
                nativeInstallCheckInputs = [ ];
              });
          }
        )
      )
    ];
  };
  update =
    _: entry:
    entry
    // {
      storePath =
        (import shellFile {
          pkgs = pkgs.pkgsCross.${entry.crossSystem}.pkgsStatic;
          withPerfetto = true;
        }).TRACE_PROCESSOR_SHELL_PATH;
    };
  rawData = builtins.mapAttrs update (builtins.fromJSON (builtins.readFile jsonFile));
in
# The store paths keep their string context, so building this also builds them.
(pkgs.formats.json { }).generate "trace_processor_shell.json" rawData
// {
  # Exposing the raw data so the JSON can be generated without needing to actually cross-compile,
  # allowing to generate the file from a non-aarch64-linux machine.
  inherit rawData;
}
