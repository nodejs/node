args@{
  pkgs ? import ./pkgs.nix { },
  ...
}:
import ../../shell.nix (
  {
    inherit pkgs;
    useSeparateDerivationForV8 = true;
    loadJSBuiltinsDynamically = false;
    devTools = [ ];
    benchmarkTools = [ ];
  }
  // pkgs.lib.optionalAttrs pkgs.stdenv.hostPlatform.isDarwin {
    withAmaro = false;
    withFFI = false;
    withLief = false;
    withSQLite = false;
    withTemporal = false;
    extraConfigFlags = [
      "--without-inspector"
      "--without-node-options"
    ];
  }
  // args
)
