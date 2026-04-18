arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "13043924aaa7375ce482ebe2494338e058282925";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "1pbv1c3syp94rh147s2nhbzfcib01blz3s7g290m43s3nk71404z";
  }) arg;
in
nixpkgs
// {
  nixfmt-tree = nixpkgs.nixfmt-tree.overrideAttrs (old: {
    patches = (old.patches or [ ]) ++ [
      (nixpkgs.fetchpatch2 {
        url = "https://github.com/numtide/treefmt/commit/b96016b4e38ffc76518087b3b0c9bbfa190d5225.patch?full_index=1";
        revert = true;
        hash = "sha256-DcxT2OGPX6Kmxhqa56DjZsSh2hoyhPyVmE17ULeryv8=";
      })
    ];
  });
}
