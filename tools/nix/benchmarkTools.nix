{
  pkgs ? import ./pkgs.nix { },
  withHttpBenchmarkDeps ? true,
}:
[
  pkgs.R
  pkgs.rPackages.ggplot2
  pkgs.rPackages.plyr
]
++ pkgs.lib.optional withHttpBenchmarkDeps pkgs.wrk
++ pkgs.lib.optional pkgs.stdenv.buildPlatform.isLinux pkgs.glibcLocales
