/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/utils/explain-eresolve.js TAP chain-conflict > explain with color, depth of 2 1`] = `
While resolving: [1mproject[22m@[1m1.2.3[22m
Found: [1m@isaacs/testing-peer-dep-conflict-chain-d[22m@[1m2.0.0[22m[2m[22m
[2mnode_modules/@isaacs/testing-peer-dep-conflict-chain-d[22m
  [1m@isaacs/testing-peer-dep-conflict-chain-d[22m@"[1m2[22m" from the root project

Could not resolve dependency:
[35mpeer[39m [1m@isaacs/testing-peer-dep-conflict-chain-d[22m@"[1m1[22m" from [1m@isaacs/testing-peer-dep-conflict-chain-c[22m@[1m1.0.0[22m[2m[22m
[2mnode_modules/@isaacs/testing-peer-dep-conflict-chain-c[22m
  [1m@isaacs/testing-peer-dep-conflict-chain-c[22m@"[1m1[22m" from the root project
`

exports[`test/lib/utils/explain-eresolve.js TAP chain-conflict > explain with no color, depth of 6 1`] = `
While resolving: project@1.2.3
Found: @isaacs/testing-peer-dep-conflict-chain-d@2.0.0
node_modules/@isaacs/testing-peer-dep-conflict-chain-d
  @isaacs/testing-peer-dep-conflict-chain-d@"2" from the root project

Could not resolve dependency:
peer @isaacs/testing-peer-dep-conflict-chain-d@"1" from @isaacs/testing-peer-dep-conflict-chain-c@1.0.0
node_modules/@isaacs/testing-peer-dep-conflict-chain-c
  @isaacs/testing-peer-dep-conflict-chain-c@"1" from the root project
`

exports[`test/lib/utils/explain-eresolve.js TAP chain-conflict > report 1`] = `
# npm resolution error report

\${TIME}

While resolving: project@1.2.3
Found: @isaacs/testing-peer-dep-conflict-chain-d@2.0.0
node_modules/@isaacs/testing-peer-dep-conflict-chain-d
  @isaacs/testing-peer-dep-conflict-chain-d@"2" from the root project

Could not resolve dependency:
peer @isaacs/testing-peer-dep-conflict-chain-d@"1" from @isaacs/testing-peer-dep-conflict-chain-c@1.0.0
node_modules/@isaacs/testing-peer-dep-conflict-chain-c
  @isaacs/testing-peer-dep-conflict-chain-c@"1" from the root project

Fix the upstream dependency conflict, or retry
this command with --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

Raw JSON explanation object:

{
  "name": "chain-conflict",
  "json": true
}

`

exports[`test/lib/utils/explain-eresolve.js TAP chain-conflict > report with color 1`] = `
While resolving: [1mproject[22m@[1m1.2.3[22m
Found: [1m@isaacs/testing-peer-dep-conflict-chain-d[22m@[1m2.0.0[22m[2m[22m
[2mnode_modules/@isaacs/testing-peer-dep-conflict-chain-d[22m
  [1m@isaacs/testing-peer-dep-conflict-chain-d[22m@"[1m2[22m" from the root project

Could not resolve dependency:
[35mpeer[39m [1m@isaacs/testing-peer-dep-conflict-chain-d[22m@"[1m1[22m" from [1m@isaacs/testing-peer-dep-conflict-chain-c[22m@[1m1.0.0[22m[2m[22m
[2mnode_modules/@isaacs/testing-peer-dep-conflict-chain-c[22m
  [1m@isaacs/testing-peer-dep-conflict-chain-c[22m@"[1m1[22m" from the root project

Fix the upstream dependency conflict, or retry
this command with --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

See \${REPORT} for a full report.
`

exports[`test/lib/utils/explain-eresolve.js TAP chain-conflict > report with no color 1`] = `
While resolving: project@1.2.3
Found: @isaacs/testing-peer-dep-conflict-chain-d@2.0.0
node_modules/@isaacs/testing-peer-dep-conflict-chain-d
  @isaacs/testing-peer-dep-conflict-chain-d@"2" from the root project

Could not resolve dependency:
peer @isaacs/testing-peer-dep-conflict-chain-d@"1" from @isaacs/testing-peer-dep-conflict-chain-c@1.0.0
node_modules/@isaacs/testing-peer-dep-conflict-chain-c
  @isaacs/testing-peer-dep-conflict-chain-c@"1" from the root project

Fix the upstream dependency conflict, or retry
this command with --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

See \${REPORT} for a full report.
`

exports[`test/lib/utils/explain-eresolve.js TAP cycleNested > explain with color, depth of 2 1`] = `
Found: [1m@isaacs/peer-dep-cycle-c[22m@[1m2.0.0[22m[2m[22m
[2mnode_modules/@isaacs/peer-dep-cycle-c[22m
  [1m@isaacs/peer-dep-cycle-c[22m@"[1m2.x[22m" from the root project

Could not resolve dependency:
[35mpeer[39m [1m@isaacs/peer-dep-cycle-b[22m@"[1m1[22m" from [1m@isaacs/peer-dep-cycle-a[22m@[1m1.0.0[22m[2m[22m
[2mnode_modules/@isaacs/peer-dep-cycle-a[22m
  [1m@isaacs/peer-dep-cycle-a[22m@"[1m1.x[22m" from the root project

Conflicting peer dependency: [1m@isaacs/peer-dep-cycle-c[22m@[1m1.0.0[22m[2m[22m
[2mnode_modules/@isaacs/peer-dep-cycle-c[22m
  [35mpeer[39m [1m@isaacs/peer-dep-cycle-c[22m@"[1m1[22m" from [1m@isaacs/peer-dep-cycle-b[22m@[1m1.0.0[22m[2m[22m
  [2mnode_modules/@isaacs/peer-dep-cycle-b[22m
    [35mpeer[39m [1m@isaacs/peer-dep-cycle-b[22m@"[1m1[22m" from [1m@isaacs/peer-dep-cycle-a[22m@[1m1.0.0[22m[2m[22m
    [2mnode_modules/@isaacs/peer-dep-cycle-a[22m
`

exports[`test/lib/utils/explain-eresolve.js TAP cycleNested > explain with no color, depth of 6 1`] = `
Found: @isaacs/peer-dep-cycle-c@2.0.0
node_modules/@isaacs/peer-dep-cycle-c
  @isaacs/peer-dep-cycle-c@"2.x" from the root project

Could not resolve dependency:
peer @isaacs/peer-dep-cycle-b@"1" from @isaacs/peer-dep-cycle-a@1.0.0
node_modules/@isaacs/peer-dep-cycle-a
  @isaacs/peer-dep-cycle-a@"1.x" from the root project

Conflicting peer dependency: @isaacs/peer-dep-cycle-c@1.0.0
node_modules/@isaacs/peer-dep-cycle-c
  peer @isaacs/peer-dep-cycle-c@"1" from @isaacs/peer-dep-cycle-b@1.0.0
  node_modules/@isaacs/peer-dep-cycle-b
    peer @isaacs/peer-dep-cycle-b@"1" from @isaacs/peer-dep-cycle-a@1.0.0
    node_modules/@isaacs/peer-dep-cycle-a
      @isaacs/peer-dep-cycle-a@"1.x" from the root project
`

exports[`test/lib/utils/explain-eresolve.js TAP cycleNested > report 1`] = `
# npm resolution error report

\${TIME}

Found: @isaacs/peer-dep-cycle-c@2.0.0
node_modules/@isaacs/peer-dep-cycle-c
  @isaacs/peer-dep-cycle-c@"2.x" from the root project

Could not resolve dependency:
peer @isaacs/peer-dep-cycle-b@"1" from @isaacs/peer-dep-cycle-a@1.0.0
node_modules/@isaacs/peer-dep-cycle-a
  @isaacs/peer-dep-cycle-a@"1.x" from the root project

Conflicting peer dependency: @isaacs/peer-dep-cycle-c@1.0.0
node_modules/@isaacs/peer-dep-cycle-c
  peer @isaacs/peer-dep-cycle-c@"1" from @isaacs/peer-dep-cycle-b@1.0.0
  node_modules/@isaacs/peer-dep-cycle-b
    peer @isaacs/peer-dep-cycle-b@"1" from @isaacs/peer-dep-cycle-a@1.0.0
    node_modules/@isaacs/peer-dep-cycle-a
      @isaacs/peer-dep-cycle-a@"1.x" from the root project

Fix the upstream dependency conflict, or retry
this command with --no-strict-peer-deps, --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

Raw JSON explanation object:

{
  "name": "cycleNested",
  "json": true
}

`

exports[`test/lib/utils/explain-eresolve.js TAP cycleNested > report with color 1`] = `
Found: [1m@isaacs/peer-dep-cycle-c[22m@[1m2.0.0[22m[2m[22m
[2mnode_modules/@isaacs/peer-dep-cycle-c[22m
  [1m@isaacs/peer-dep-cycle-c[22m@"[1m2.x[22m" from the root project

Could not resolve dependency:
[35mpeer[39m [1m@isaacs/peer-dep-cycle-b[22m@"[1m1[22m" from [1m@isaacs/peer-dep-cycle-a[22m@[1m1.0.0[22m[2m[22m
[2mnode_modules/@isaacs/peer-dep-cycle-a[22m
  [1m@isaacs/peer-dep-cycle-a[22m@"[1m1.x[22m" from the root project

Conflicting peer dependency: [1m@isaacs/peer-dep-cycle-c[22m@[1m1.0.0[22m[2m[22m
[2mnode_modules/@isaacs/peer-dep-cycle-c[22m
  [35mpeer[39m [1m@isaacs/peer-dep-cycle-c[22m@"[1m1[22m" from [1m@isaacs/peer-dep-cycle-b[22m@[1m1.0.0[22m[2m[22m
  [2mnode_modules/@isaacs/peer-dep-cycle-b[22m
    [35mpeer[39m [1m@isaacs/peer-dep-cycle-b[22m@"[1m1[22m" from [1m@isaacs/peer-dep-cycle-a[22m@[1m1.0.0[22m[2m[22m
    [2mnode_modules/@isaacs/peer-dep-cycle-a[22m
      [1m@isaacs/peer-dep-cycle-a[22m@"[1m1.x[22m" from the root project

Fix the upstream dependency conflict, or retry
this command with --no-strict-peer-deps, --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

See \${REPORT} for a full report.
`

exports[`test/lib/utils/explain-eresolve.js TAP cycleNested > report with no color 1`] = `
Found: @isaacs/peer-dep-cycle-c@2.0.0
node_modules/@isaacs/peer-dep-cycle-c
  @isaacs/peer-dep-cycle-c@"2.x" from the root project

Could not resolve dependency:
peer @isaacs/peer-dep-cycle-b@"1" from @isaacs/peer-dep-cycle-a@1.0.0
node_modules/@isaacs/peer-dep-cycle-a
  @isaacs/peer-dep-cycle-a@"1.x" from the root project

Conflicting peer dependency: @isaacs/peer-dep-cycle-c@1.0.0
node_modules/@isaacs/peer-dep-cycle-c
  peer @isaacs/peer-dep-cycle-c@"1" from @isaacs/peer-dep-cycle-b@1.0.0
  node_modules/@isaacs/peer-dep-cycle-b
    peer @isaacs/peer-dep-cycle-b@"1" from @isaacs/peer-dep-cycle-a@1.0.0
    node_modules/@isaacs/peer-dep-cycle-a
      @isaacs/peer-dep-cycle-a@"1.x" from the root project

Fix the upstream dependency conflict, or retry
this command with --no-strict-peer-deps, --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

See \${REPORT} for a full report.
`

exports[`test/lib/utils/explain-eresolve.js TAP gatsby > explain with color, depth of 2 1`] = `
While resolving: [1mgatsby-recipes[22m@[1m0.2.31[22m
Found: [1mink[22m@[1m3.0.0-7[22m[2m[22m
[2mnode_modules/ink[22m
  [33mdev[39m [1mink[22m@"[1mnext[22m" from [1mgatsby-recipes[22m@[1m0.2.31[22m[2m[22m
  [2mnode_modules/gatsby-recipes[22m
    [1mgatsby-recipes[22m@"[1m^0.2.31[22m" from [1mgatsby-cli[22m@[1m2.12.107[22m[2m[22m
    [2mnode_modules/gatsby-cli[22m

Could not resolve dependency:
[35mpeer[39m [1mink[22m@"[1m>=2.0.0[22m" from [1mink-box[22m@[1m1.0.0[22m[2m[22m
[2mnode_modules/ink-box[22m
  [1mink-box[22m@"[1m^1.0.0[22m" from [1mgatsby-recipes[22m@[1m0.2.31[22m[2m[22m
  [2mnode_modules/gatsby-recipes[22m
`

exports[`test/lib/utils/explain-eresolve.js TAP gatsby > explain with no color, depth of 6 1`] = `
While resolving: gatsby-recipes@0.2.31
Found: ink@3.0.0-7
node_modules/ink
  dev ink@"next" from gatsby-recipes@0.2.31
  node_modules/gatsby-recipes
    gatsby-recipes@"^0.2.31" from gatsby-cli@2.12.107
    node_modules/gatsby-cli
      gatsby-cli@"^2.12.107" from gatsby@2.24.74
      node_modules/gatsby
        gatsby@"" from the root project

Could not resolve dependency:
peer ink@">=2.0.0" from ink-box@1.0.0
node_modules/ink-box
  ink-box@"^1.0.0" from gatsby-recipes@0.2.31
  node_modules/gatsby-recipes
    gatsby-recipes@"^0.2.31" from gatsby-cli@2.12.107
    node_modules/gatsby-cli
      gatsby-cli@"^2.12.107" from gatsby@2.24.74
      node_modules/gatsby
        gatsby@"" from the root project
`

exports[`test/lib/utils/explain-eresolve.js TAP gatsby > report 1`] = `
# npm resolution error report

\${TIME}

While resolving: gatsby-recipes@0.2.31
Found: ink@3.0.0-7
node_modules/ink
  dev ink@"next" from gatsby-recipes@0.2.31
  node_modules/gatsby-recipes
    gatsby-recipes@"^0.2.31" from gatsby-cli@2.12.107
    node_modules/gatsby-cli
      gatsby-cli@"^2.12.107" from gatsby@2.24.74
      node_modules/gatsby
        gatsby@"" from the root project

Could not resolve dependency:
peer ink@">=2.0.0" from ink-box@1.0.0
node_modules/ink-box
  ink-box@"^1.0.0" from gatsby-recipes@0.2.31
  node_modules/gatsby-recipes
    gatsby-recipes@"^0.2.31" from gatsby-cli@2.12.107
    node_modules/gatsby-cli
      gatsby-cli@"^2.12.107" from gatsby@2.24.74
      node_modules/gatsby
        gatsby@"" from the root project

Fix the upstream dependency conflict, or retry
this command with --no-strict-peer-deps, --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

Raw JSON explanation object:

{
  "name": "gatsby",
  "json": true
}

`

exports[`test/lib/utils/explain-eresolve.js TAP gatsby > report with color 1`] = `
While resolving: [1mgatsby-recipes[22m@[1m0.2.31[22m
Found: [1mink[22m@[1m3.0.0-7[22m[2m[22m
[2mnode_modules/ink[22m
  [33mdev[39m [1mink[22m@"[1mnext[22m" from [1mgatsby-recipes[22m@[1m0.2.31[22m[2m[22m
  [2mnode_modules/gatsby-recipes[22m
    [1mgatsby-recipes[22m@"[1m^0.2.31[22m" from [1mgatsby-cli[22m@[1m2.12.107[22m[2m[22m
    [2mnode_modules/gatsby-cli[22m
      [1mgatsby-cli[22m@"[1m^2.12.107[22m" from [1mgatsby[22m@[1m2.24.74[22m[2m[22m
      [2mnode_modules/gatsby[22m
        [1mgatsby[22m@"" from the root project

Could not resolve dependency:
[35mpeer[39m [1mink[22m@"[1m>=2.0.0[22m" from [1mink-box[22m@[1m1.0.0[22m[2m[22m
[2mnode_modules/ink-box[22m
  [1mink-box[22m@"[1m^1.0.0[22m" from [1mgatsby-recipes[22m@[1m0.2.31[22m[2m[22m
  [2mnode_modules/gatsby-recipes[22m
    [1mgatsby-recipes[22m@"[1m^0.2.31[22m" from [1mgatsby-cli[22m@[1m2.12.107[22m[2m[22m
    [2mnode_modules/gatsby-cli[22m
      [1mgatsby-cli[22m@"[1m^2.12.107[22m" from [1mgatsby[22m@[1m2.24.74[22m[2m[22m
      [2mnode_modules/gatsby[22m

Fix the upstream dependency conflict, or retry
this command with --no-strict-peer-deps, --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

See \${REPORT} for a full report.
`

exports[`test/lib/utils/explain-eresolve.js TAP gatsby > report with no color 1`] = `
While resolving: gatsby-recipes@0.2.31
Found: ink@3.0.0-7
node_modules/ink
  dev ink@"next" from gatsby-recipes@0.2.31
  node_modules/gatsby-recipes
    gatsby-recipes@"^0.2.31" from gatsby-cli@2.12.107
    node_modules/gatsby-cli
      gatsby-cli@"^2.12.107" from gatsby@2.24.74
      node_modules/gatsby
        gatsby@"" from the root project

Could not resolve dependency:
peer ink@">=2.0.0" from ink-box@1.0.0
node_modules/ink-box
  ink-box@"^1.0.0" from gatsby-recipes@0.2.31
  node_modules/gatsby-recipes
    gatsby-recipes@"^0.2.31" from gatsby-cli@2.12.107
    node_modules/gatsby-cli
      gatsby-cli@"^2.12.107" from gatsby@2.24.74
      node_modules/gatsby

Fix the upstream dependency conflict, or retry
this command with --no-strict-peer-deps, --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

See \${REPORT} for a full report.
`

exports[`test/lib/utils/explain-eresolve.js TAP no current node, but has current edge > explain with color, depth of 2 1`] = `
While resolving: [1meslint[22m@[1m7.22.0[22m
Found: [33mdev[39m [1meslint[22m@"[1mfile:.[22m" from the root project

Could not resolve dependency:
[35mpeer[39m [1meslint[22m@"[1m^6.0.0[22m" from [1meslint-plugin-jsdoc[22m@[1m22.2.0[22m[2m[22m
[2mnode_modules/eslint-plugin-jsdoc[22m
  [33mdev[39m [1meslint-plugin-jsdoc[22m@"[1m^22.1.0[22m" from the root project
`

exports[`test/lib/utils/explain-eresolve.js TAP no current node, but has current edge > explain with no color, depth of 6 1`] = `
While resolving: eslint@7.22.0
Found: dev eslint@"file:." from the root project

Could not resolve dependency:
peer eslint@"^6.0.0" from eslint-plugin-jsdoc@22.2.0
node_modules/eslint-plugin-jsdoc
  dev eslint-plugin-jsdoc@"^22.1.0" from the root project
`

exports[`test/lib/utils/explain-eresolve.js TAP no current node, but has current edge > report 1`] = `
# npm resolution error report

\${TIME}

While resolving: eslint@7.22.0
Found: dev eslint@"file:." from the root project

Could not resolve dependency:
peer eslint@"^6.0.0" from eslint-plugin-jsdoc@22.2.0
node_modules/eslint-plugin-jsdoc
  dev eslint-plugin-jsdoc@"^22.1.0" from the root project

Fix the upstream dependency conflict, or retry
this command with --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

Raw JSON explanation object:

{
  "name": "no current node, but has current edge",
  "json": true
}

`

exports[`test/lib/utils/explain-eresolve.js TAP no current node, but has current edge > report with color 1`] = `
While resolving: [1meslint[22m@[1m7.22.0[22m
Found: [33mdev[39m [1meslint[22m@"[1mfile:.[22m" from the root project

Could not resolve dependency:
[35mpeer[39m [1meslint[22m@"[1m^6.0.0[22m" from [1meslint-plugin-jsdoc[22m@[1m22.2.0[22m[2m[22m
[2mnode_modules/eslint-plugin-jsdoc[22m
  [33mdev[39m [1meslint-plugin-jsdoc[22m@"[1m^22.1.0[22m" from the root project

Fix the upstream dependency conflict, or retry
this command with --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

See \${REPORT} for a full report.
`

exports[`test/lib/utils/explain-eresolve.js TAP no current node, but has current edge > report with no color 1`] = `
While resolving: eslint@7.22.0
Found: dev eslint@"file:." from the root project

Could not resolve dependency:
peer eslint@"^6.0.0" from eslint-plugin-jsdoc@22.2.0
node_modules/eslint-plugin-jsdoc
  dev eslint-plugin-jsdoc@"^22.1.0" from the root project

Fix the upstream dependency conflict, or retry
this command with --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

See \${REPORT} for a full report.
`

exports[`test/lib/utils/explain-eresolve.js TAP no current node, no current edge, idk > explain with color, depth of 2 1`] = `
While resolving: [1meslint[22m@[1m7.22.0[22m

Could not resolve dependency:
[35mpeer[39m [1meslint[22m@"[1m^6.0.0[22m" from [1meslint-plugin-jsdoc[22m@[1m22.2.0[22m[2m[22m
[2mnode_modules/eslint-plugin-jsdoc[22m
  [33mdev[39m [1meslint-plugin-jsdoc[22m@"[1m^22.1.0[22m" from the root project
`

exports[`test/lib/utils/explain-eresolve.js TAP no current node, no current edge, idk > explain with no color, depth of 6 1`] = `
While resolving: eslint@7.22.0

Could not resolve dependency:
peer eslint@"^6.0.0" from eslint-plugin-jsdoc@22.2.0
node_modules/eslint-plugin-jsdoc
  dev eslint-plugin-jsdoc@"^22.1.0" from the root project
`

exports[`test/lib/utils/explain-eresolve.js TAP no current node, no current edge, idk > report 1`] = `
# npm resolution error report

\${TIME}

While resolving: eslint@7.22.0

Could not resolve dependency:
peer eslint@"^6.0.0" from eslint-plugin-jsdoc@22.2.0
node_modules/eslint-plugin-jsdoc
  dev eslint-plugin-jsdoc@"^22.1.0" from the root project

Fix the upstream dependency conflict, or retry
this command with --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

Raw JSON explanation object:

{
  "name": "no current node, no current edge, idk",
  "json": true
}

`

exports[`test/lib/utils/explain-eresolve.js TAP no current node, no current edge, idk > report with color 1`] = `
While resolving: [1meslint[22m@[1m7.22.0[22m

Could not resolve dependency:
[35mpeer[39m [1meslint[22m@"[1m^6.0.0[22m" from [1meslint-plugin-jsdoc[22m@[1m22.2.0[22m[2m[22m
[2mnode_modules/eslint-plugin-jsdoc[22m
  [33mdev[39m [1meslint-plugin-jsdoc[22m@"[1m^22.1.0[22m" from the root project

Fix the upstream dependency conflict, or retry
this command with --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

See \${REPORT} for a full report.
`

exports[`test/lib/utils/explain-eresolve.js TAP no current node, no current edge, idk > report with no color 1`] = `
While resolving: eslint@7.22.0

Could not resolve dependency:
peer eslint@"^6.0.0" from eslint-plugin-jsdoc@22.2.0
node_modules/eslint-plugin-jsdoc
  dev eslint-plugin-jsdoc@"^22.1.0" from the root project

Fix the upstream dependency conflict, or retry
this command with --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

See \${REPORT} for a full report.
`

exports[`test/lib/utils/explain-eresolve.js TAP withShrinkwrap > explain with color, depth of 2 1`] = `
While resolving: [1m@isaacs/peer-dep-cycle-b[22m@[1m1.0.0[22m
Found: [1m@isaacs/peer-dep-cycle-c[22m@[1m2.0.0[22m[2m[22m
[2mnode_modules/@isaacs/peer-dep-cycle-c[22m
  [1m@isaacs/peer-dep-cycle-c[22m@"[1m2.x[22m" from the root project

Could not resolve dependency:
[35mpeer[39m [1m@isaacs/peer-dep-cycle-c[22m@"[1m1[22m" from [1m@isaacs/peer-dep-cycle-b[22m@[1m1.0.0[22m[2m[22m
[2mnode_modules/@isaacs/peer-dep-cycle-b[22m
  [35mpeer[39m [1m@isaacs/peer-dep-cycle-b[22m@"[1m1[22m" from [1m@isaacs/peer-dep-cycle-a[22m@[1m1.0.0[22m[2m[22m
  [2mnode_modules/@isaacs/peer-dep-cycle-a[22m
`

exports[`test/lib/utils/explain-eresolve.js TAP withShrinkwrap > explain with no color, depth of 6 1`] = `
While resolving: @isaacs/peer-dep-cycle-b@1.0.0
Found: @isaacs/peer-dep-cycle-c@2.0.0
node_modules/@isaacs/peer-dep-cycle-c
  @isaacs/peer-dep-cycle-c@"2.x" from the root project

Could not resolve dependency:
peer @isaacs/peer-dep-cycle-c@"1" from @isaacs/peer-dep-cycle-b@1.0.0
node_modules/@isaacs/peer-dep-cycle-b
  peer @isaacs/peer-dep-cycle-b@"1" from @isaacs/peer-dep-cycle-a@1.0.0
  node_modules/@isaacs/peer-dep-cycle-a
    @isaacs/peer-dep-cycle-a@"1.x" from the root project
`

exports[`test/lib/utils/explain-eresolve.js TAP withShrinkwrap > report 1`] = `
# npm resolution error report

\${TIME}

While resolving: @isaacs/peer-dep-cycle-b@1.0.0
Found: @isaacs/peer-dep-cycle-c@2.0.0
node_modules/@isaacs/peer-dep-cycle-c
  @isaacs/peer-dep-cycle-c@"2.x" from the root project

Could not resolve dependency:
peer @isaacs/peer-dep-cycle-c@"1" from @isaacs/peer-dep-cycle-b@1.0.0
node_modules/@isaacs/peer-dep-cycle-b
  peer @isaacs/peer-dep-cycle-b@"1" from @isaacs/peer-dep-cycle-a@1.0.0
  node_modules/@isaacs/peer-dep-cycle-a
    @isaacs/peer-dep-cycle-a@"1.x" from the root project

Fix the upstream dependency conflict, or retry
this command with --no-strict-peer-deps, --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

Raw JSON explanation object:

{
  "name": "withShrinkwrap",
  "json": true
}

`

exports[`test/lib/utils/explain-eresolve.js TAP withShrinkwrap > report with color 1`] = `
While resolving: [1m@isaacs/peer-dep-cycle-b[22m@[1m1.0.0[22m
Found: [1m@isaacs/peer-dep-cycle-c[22m@[1m2.0.0[22m[2m[22m
[2mnode_modules/@isaacs/peer-dep-cycle-c[22m
  [1m@isaacs/peer-dep-cycle-c[22m@"[1m2.x[22m" from the root project

Could not resolve dependency:
[35mpeer[39m [1m@isaacs/peer-dep-cycle-c[22m@"[1m1[22m" from [1m@isaacs/peer-dep-cycle-b[22m@[1m1.0.0[22m[2m[22m
[2mnode_modules/@isaacs/peer-dep-cycle-b[22m
  [35mpeer[39m [1m@isaacs/peer-dep-cycle-b[22m@"[1m1[22m" from [1m@isaacs/peer-dep-cycle-a[22m@[1m1.0.0[22m[2m[22m
  [2mnode_modules/@isaacs/peer-dep-cycle-a[22m
    [1m@isaacs/peer-dep-cycle-a[22m@"[1m1.x[22m" from the root project

Fix the upstream dependency conflict, or retry
this command with --no-strict-peer-deps, --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

See \${REPORT} for a full report.
`

exports[`test/lib/utils/explain-eresolve.js TAP withShrinkwrap > report with no color 1`] = `
While resolving: @isaacs/peer-dep-cycle-b@1.0.0
Found: @isaacs/peer-dep-cycle-c@2.0.0
node_modules/@isaacs/peer-dep-cycle-c
  @isaacs/peer-dep-cycle-c@"2.x" from the root project

Could not resolve dependency:
peer @isaacs/peer-dep-cycle-c@"1" from @isaacs/peer-dep-cycle-b@1.0.0
node_modules/@isaacs/peer-dep-cycle-b
  peer @isaacs/peer-dep-cycle-b@"1" from @isaacs/peer-dep-cycle-a@1.0.0
  node_modules/@isaacs/peer-dep-cycle-a
    @isaacs/peer-dep-cycle-a@"1.x" from the root project

Fix the upstream dependency conflict, or retry
this command with --no-strict-peer-deps, --force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.

See \${REPORT} for a full report.
`
