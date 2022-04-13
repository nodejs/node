/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/outdated.js TAP aliases > should display aliased outdated dep output 1`] = `

Package         Current  Wanted  Latest  Location          Depended by
cat:dog@latest    1.0.0   2.0.0   2.0.0  node_modules/cat  tap-testdir-outdated-aliases
`

exports[`test/lib/commands/outdated.js TAP should display outdated deps outdated --all > must match snapshot 1`] = `

Package  Current  Wanted  Latest  Location           Depended by
cat        1.0.0   1.0.1   1.0.1  node_modules/cat   tap-testdir-outdated-should-display-outdated-deps
chai       1.0.0   1.0.1   1.0.1  node_modules/chai  tap-testdir-outdated-should-display-outdated-deps
dog        1.0.1   1.0.1   2.0.0  node_modules/dog   tap-testdir-outdated-should-display-outdated-deps
theta    MISSING   1.0.1   1.0.1  -                  tap-testdir-outdated-should-display-outdated-deps
`

exports[`test/lib/commands/outdated.js TAP should display outdated deps outdated --json --long > must match snapshot 1`] = `

{
  "cat": {
    "current": "1.0.0",
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "tap-testdir-outdated-should-display-outdated-deps",
    "location": "{CWD}/test/lib/commands/tap-testdir-outdated-should-display-outdated-deps/node_modules/cat",
    "type": "dependencies"
  },
  "chai": {
    "current": "1.0.0",
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "tap-testdir-outdated-should-display-outdated-deps",
    "location": "{CWD}/test/lib/commands/tap-testdir-outdated-should-display-outdated-deps/node_modules/chai",
    "type": "peerDependencies"
  },
  "dog": {
    "current": "1.0.1",
    "wanted": "1.0.1",
    "latest": "2.0.0",
    "dependent": "tap-testdir-outdated-should-display-outdated-deps",
    "location": "{CWD}/test/lib/commands/tap-testdir-outdated-should-display-outdated-deps/node_modules/dog",
    "type": "dependencies"
  },
  "theta": {
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "tap-testdir-outdated-should-display-outdated-deps",
    "type": "dependencies"
  }
}
`

exports[`test/lib/commands/outdated.js TAP should display outdated deps outdated --json > must match snapshot 1`] = `

{
  "cat": {
    "current": "1.0.0",
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "tap-testdir-outdated-should-display-outdated-deps",
    "location": "{CWD}/test/lib/commands/tap-testdir-outdated-should-display-outdated-deps/node_modules/cat"
  },
  "chai": {
    "current": "1.0.0",
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "tap-testdir-outdated-should-display-outdated-deps",
    "location": "{CWD}/test/lib/commands/tap-testdir-outdated-should-display-outdated-deps/node_modules/chai"
  },
  "dog": {
    "current": "1.0.1",
    "wanted": "1.0.1",
    "latest": "2.0.0",
    "dependent": "tap-testdir-outdated-should-display-outdated-deps",
    "location": "{CWD}/test/lib/commands/tap-testdir-outdated-should-display-outdated-deps/node_modules/dog"
  },
  "theta": {
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "tap-testdir-outdated-should-display-outdated-deps"
  }
}
`

exports[`test/lib/commands/outdated.js TAP should display outdated deps outdated --long > must match snapshot 1`] = `

Package  Current  Wanted  Latest  Location           Depended by                                        Package Type      Homepage
cat        1.0.0   1.0.1   1.0.1  node_modules/cat   tap-testdir-outdated-should-display-outdated-deps  dependencies
chai       1.0.0   1.0.1   1.0.1  node_modules/chai  tap-testdir-outdated-should-display-outdated-deps  peerDependencies
dog        1.0.1   1.0.1   2.0.0  node_modules/dog   tap-testdir-outdated-should-display-outdated-deps  dependencies
theta    MISSING   1.0.1   1.0.1  -                  tap-testdir-outdated-should-display-outdated-deps  dependencies
`

exports[`test/lib/commands/outdated.js TAP should display outdated deps outdated --omit=dev --omit=peer > must match snapshot 1`] = `

[4mPackage[24m  [4mCurrent[24m  [4mWanted[24m  [4mLatest[24m  [4mLocation[24m          [4mDepended by[24m
[31mcat[39m        1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/cat  tap-testdir-outdated-should-display-outdated-deps
[33mdog[39m        1.0.1   [32m1.0.1[39m   [35m2.0.0[39m  node_modules/dog  tap-testdir-outdated-should-display-outdated-deps
[31mtheta[39m    MISSING   [32m1.0.1[39m   [35m1.0.1[39m  -                 tap-testdir-outdated-should-display-outdated-deps
`

exports[`test/lib/commands/outdated.js TAP should display outdated deps outdated --omit=dev > must match snapshot 1`] = `

[4mPackage[24m  [4mCurrent[24m  [4mWanted[24m  [4mLatest[24m  [4mLocation[24m           [4mDepended by[24m
[31mcat[39m        1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/cat   tap-testdir-outdated-should-display-outdated-deps
[31mchai[39m       1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/chai  tap-testdir-outdated-should-display-outdated-deps
[33mdog[39m        1.0.1   [32m1.0.1[39m   [35m2.0.0[39m  node_modules/dog   tap-testdir-outdated-should-display-outdated-deps
[31mtheta[39m    MISSING   [32m1.0.1[39m   [35m1.0.1[39m  -                  tap-testdir-outdated-should-display-outdated-deps
`

exports[`test/lib/commands/outdated.js TAP should display outdated deps outdated --omit=prod > must match snapshot 1`] = `

[4mPackage[24m  [4mCurrent[24m  [4mWanted[24m  [4mLatest[24m  [4mLocation[24m           [4mDepended by[24m
[31mcat[39m        1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/cat   tap-testdir-outdated-should-display-outdated-deps
[31mchai[39m       1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/chai  tap-testdir-outdated-should-display-outdated-deps
[33mdog[39m        1.0.1   [32m1.0.1[39m   [35m2.0.0[39m  node_modules/dog   tap-testdir-outdated-should-display-outdated-deps
`

exports[`test/lib/commands/outdated.js TAP should display outdated deps outdated --parseable --long > must match snapshot 1`] = `

{CWD}/test/lib/commands/tap-testdir-outdated-should-display-outdated-deps/node_modules/cat:cat@1.0.1:cat@1.0.0:cat@1.0.1:tap-testdir-outdated-should-display-outdated-deps:dependencies:
{CWD}/test/lib/commands/tap-testdir-outdated-should-display-outdated-deps/node_modules/chai:chai@1.0.1:chai@1.0.0:chai@1.0.1:tap-testdir-outdated-should-display-outdated-deps:peerDependencies:
{CWD}/test/lib/commands/tap-testdir-outdated-should-display-outdated-deps/node_modules/dog:dog@1.0.1:dog@1.0.1:dog@2.0.0:tap-testdir-outdated-should-display-outdated-deps:dependencies:
:theta@1.0.1:MISSING:theta@1.0.1:tap-testdir-outdated-should-display-outdated-deps:dependencies:
`

exports[`test/lib/commands/outdated.js TAP should display outdated deps outdated --parseable > must match snapshot 1`] = `

{CWD}/test/lib/commands/tap-testdir-outdated-should-display-outdated-deps/node_modules/cat:cat@1.0.1:cat@1.0.0:cat@1.0.1:tap-testdir-outdated-should-display-outdated-deps
{CWD}/test/lib/commands/tap-testdir-outdated-should-display-outdated-deps/node_modules/chai:chai@1.0.1:chai@1.0.0:chai@1.0.1:tap-testdir-outdated-should-display-outdated-deps
{CWD}/test/lib/commands/tap-testdir-outdated-should-display-outdated-deps/node_modules/dog:dog@1.0.1:dog@1.0.1:dog@2.0.0:tap-testdir-outdated-should-display-outdated-deps
:theta@1.0.1:MISSING:theta@1.0.1:tap-testdir-outdated-should-display-outdated-deps
`

exports[`test/lib/commands/outdated.js TAP should display outdated deps outdated > must match snapshot 1`] = `

[4mPackage[24m  [4mCurrent[24m  [4mWanted[24m  [4mLatest[24m  [4mLocation[24m           [4mDepended by[24m
[31mcat[39m        1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/cat   tap-testdir-outdated-should-display-outdated-deps
[31mchai[39m       1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/chai  tap-testdir-outdated-should-display-outdated-deps
[33mdog[39m        1.0.1   [32m1.0.1[39m   [35m2.0.0[39m  node_modules/dog   tap-testdir-outdated-should-display-outdated-deps
[31mtheta[39m    MISSING   [32m1.0.1[39m   [35m1.0.1[39m  -                  tap-testdir-outdated-should-display-outdated-deps
`

exports[`test/lib/commands/outdated.js TAP should display outdated deps outdated global > must match snapshot 1`] = `

Package  Current  Wanted  Latest  Location          Depended by
cat        1.0.0   1.0.1   1.0.1  node_modules/cat  global
`

exports[`test/lib/commands/outdated.js TAP should display outdated deps outdated specific dep > must match snapshot 1`] = `

Package  Current  Wanted  Latest  Location          Depended by
cat        1.0.0   1.0.1   1.0.1  node_modules/cat  tap-testdir-outdated-should-display-outdated-deps
`

exports[`test/lib/commands/outdated.js TAP workspaces > should display all dependencies 1`] = `

Package  Current  Wanted  Latest  Location           Depended by
cat        1.0.0   1.0.1   1.0.1  node_modules/cat   a@1.0.0
chai       1.0.0   1.0.1   1.0.1  node_modules/chai  foo
dog        1.0.1   1.0.1   2.0.0  node_modules/dog   tap-testdir-outdated-workspaces
theta    MISSING   1.0.1   1.0.1  -                  c@1.0.0
`

exports[`test/lib/commands/outdated.js TAP workspaces > should display json results filtered by ws 1`] = `

{
  "cat": {
    "current": "1.0.0",
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "a",
    "location": "{CWD}/test/lib/commands/tap-testdir-outdated-workspaces/node_modules/cat"
  }
}
`

exports[`test/lib/commands/outdated.js TAP workspaces > should display missing deps when filtering by ws 1`] = `

Package  Current  Wanted  Latest  Location  Depended by
theta    MISSING   1.0.1   1.0.1  -         c@1.0.0
`

exports[`test/lib/commands/outdated.js TAP workspaces > should display nested deps when filtering by ws and using --all 1`] = `

Package  Current  Wanted  Latest  Location           Depended by
cat        1.0.0   1.0.1   1.0.1  node_modules/cat   a@1.0.0
chai       1.0.0   1.0.1   1.0.1  node_modules/chai  foo
`

exports[`test/lib/commands/outdated.js TAP workspaces > should display no results if ws has no deps to display 1`] = `

`

exports[`test/lib/commands/outdated.js TAP workspaces > should display only root outdated when ws disabled 1`] = `

`

exports[`test/lib/commands/outdated.js TAP workspaces > should display parseable results filtered by ws 1`] = `

{CWD}/test/lib/commands/tap-testdir-outdated-workspaces/node_modules/cat:cat@1.0.1:cat@1.0.0:cat@1.0.1:a
`

exports[`test/lib/commands/outdated.js TAP workspaces > should display results filtered by ws 1`] = `

Package  Current  Wanted  Latest  Location          Depended by
cat        1.0.0   1.0.1   1.0.1  node_modules/cat  a@1.0.0
`

exports[`test/lib/commands/outdated.js TAP workspaces > should display ws outdated deps human output 1`] = `

Package  Current  Wanted  Latest  Location          Depended by
cat        1.0.0   1.0.1   1.0.1  node_modules/cat  a@1.0.0
dog        1.0.1   1.0.1   2.0.0  node_modules/dog  tap-testdir-outdated-workspaces
theta    MISSING   1.0.1   1.0.1  -                 c@1.0.0
`

exports[`test/lib/commands/outdated.js TAP workspaces > should display ws outdated deps json output 1`] = `

{
  "cat": {
    "current": "1.0.0",
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "a",
    "location": "{CWD}/test/lib/commands/tap-testdir-outdated-workspaces/node_modules/cat"
  },
  "dog": {
    "current": "1.0.1",
    "wanted": "1.0.1",
    "latest": "2.0.0",
    "dependent": "tap-testdir-outdated-workspaces",
    "location": "{CWD}/test/lib/commands/tap-testdir-outdated-workspaces/node_modules/dog"
  },
  "theta": {
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "c"
  }
}
`

exports[`test/lib/commands/outdated.js TAP workspaces > should display ws outdated deps parseable output 1`] = `

{CWD}/test/lib/commands/tap-testdir-outdated-workspaces/node_modules/cat:cat@1.0.1:cat@1.0.0:cat@1.0.1:a
{CWD}/test/lib/commands/tap-testdir-outdated-workspaces/node_modules/dog:dog@1.0.1:dog@1.0.1:dog@2.0.0:tap-testdir-outdated-workspaces
:theta@1.0.1:MISSING:theta@1.0.1:c
`

exports[`test/lib/commands/outdated.js TAP workspaces > should highlight ws in dependend by section 1`] = `

[4mPackage[24m  [4mCurrent[24m  [4mWanted[24m  [4mLatest[24m  [4mLocation[24m          [4mDepended by[24m
[31mcat[39m        1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/cat  [32ma@1.0.0[39m
[33mdog[39m        1.0.1   [32m1.0.1[39m   [35m2.0.0[39m  node_modules/dog  tap-testdir-outdated-workspaces
[31mtheta[39m    MISSING   [32m1.0.1[39m   [35m1.0.1[39m  -                 [32mc@1.0.0[39m
`
