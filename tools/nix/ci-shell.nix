args@{
  pkgs ? import ./pkgs.nix { },
  ...
}:
(import ../../shell.nix (
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
      extraConfigFlags = pkgs.lib.optional pkgs.stdenv.hostPlatform.isAarch "--error-on-warn" ++ [
        "--without-inspector"
        "--without-node-options"
      ];
    }
  )
  // args
)).overrideAttrs
  (old: {
    # Make shared deps reachable only through pkg-config, so regressions aren't
    # hidden by cc-wrapper's -isystem/-L for every buildInput.
    buildInputs = map (
      dep:
      pkgs.runCommand
        "${
          dep.name or (
            let
              name = baseNameOf (toString dep);
            in
            if builtins.stringLength name > 35 then builtins.substring 33 (-1) name else name
          )
        }-pkg-config-only"
        {
          __structuredAttrs = true;
          strictDeps = true;
          nativeBuildInputs = [ pkgs.pkgconf ];
          buildInputs = [ dep ];
          outputChecks.out.disallowedRequisites = [ pkgs.pkgconf ];
        }
        ''
          mkdir -p "$out/lib/pkgconfig"
          for pc in $(pkgconf --env-only --list-package-names); do
            PC_PATH="$(pkgconf --path "$pc")"
            [ -z "$PC_PATH" ] || ln -s "$PC_PATH" "$out/lib/pkgconfig/"
          done
        ''
    ) old.buildInputs;
  })
