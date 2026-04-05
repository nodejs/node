arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "a6522db5b947cd7026a40d02acc3ca261364e9c8";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0md4mhjs9vmb261hqzz5x1nq7a5nw0lhc0zkwh8550g61f6q5n67";
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
