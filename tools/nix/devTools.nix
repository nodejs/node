{
  pkgs ? import ./pkgs.nix { },
  ncu-path ? null, # Provide this if you want to use a local version of NCU
}:
[
  pkgs.curl
  pkgs.gh
  pkgs.git
  pkgs.jq
  pkgs.shellcheck
]
++ (
  if (ncu-path == null) then
    [ pkgs.node-core-utils ]
  else
    [
      (pkgs.writeShellScriptBin "git-node" "exec \"${ncu-path}/bin/git-node.js\" \"$@\"")
      (pkgs.writeShellScriptBin "ncu-ci" "exec \"${ncu-path}/bin/ncu-ci.js\" \"$@\"")
      (pkgs.writeShellScriptBin "ncu-config" "exec \"${ncu-path}/bin/ncu-config.js\" \"$@\"")
    ]
)
