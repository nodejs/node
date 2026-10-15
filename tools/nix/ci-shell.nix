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
    extraConfigFlags = [
      "--debug-node"
      "--error-on-warn"
    ];
  }
  // pkgs.lib.optionalAttrs pkgs.stdenv.hostPlatform.isDarwin (
    # Disable optional features on Darwin for coverage and faster CI.
    builtins.mapAttrs (n: v: false) (
      pkgs.lib.filterAttrs (n: v: builtins.match "with[A-Z].+" n != null) (
        builtins.functionArgs (import ../../shell.nix)
      )
    )
    // {
      withSSL = true; # no-SSL coverage is ensured by Jenkins
      extraConfigFlags = [
        "--error-on-warn"
        "--without-inspector"
        "--without-node-options"
      ];
    }
  )
  // args
)
