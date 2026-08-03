{
  pkgs ? import ./pkgs.nix { },
}:
[
  pkgs.R
  pkgs.rPackages.ggplot2
  pkgs.rPackages.plyr
  pkgs.wrk
]
