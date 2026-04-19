arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "b86751bc4085f48661017fa226dee99fab6c651b";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0z1xwfdy3blm5n06lyabyjhadiy79rbm5z4kf309z85kg65mih3b";
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
