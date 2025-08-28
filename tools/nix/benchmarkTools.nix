{
  pkgs ? import ./pkgs.nix { },
}:
[
  pkgs.R
  pkgs.rPackages.ggplot2
  pkgs.rPackages.plyr
  pkgs.wrk
]
++ pkgs.lib.optional pkgs.stdenv.buildPlatform.isLinux pkgs.glibcLocales
