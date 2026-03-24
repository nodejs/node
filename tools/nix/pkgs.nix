arg:
let
  repo = "https://github.com/NixOS/nixpkgs";
  rev = "9cf7092bdd603554bd8b63c216e8943cf9b12512";
  nixpkgs = import (builtins.fetchTarball {
    url = "${repo}/archive/${rev}.tar.gz";
    sha256 = "0bfq9cjbp8ywrdp03ji8mak5b20aa5cn8l04vvfrjybdc4q6znpn";
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
