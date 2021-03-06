/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/outdated.js TAP should display outdated deps outdated --all > must match snapshot 1`] = `

Package  Current  Wanted  Latest  Location            Depended by
alpha      1.0.0   1.0.1   1.0.1  node_modules/alpha  outdated-should-display-outdated-deps
beta       1.0.0   1.0.1   1.0.1  node_modules/beta   outdated-should-display-outdated-deps
gamma      1.0.1   1.0.1   2.0.0  node_modules/gamma  outdated-should-display-outdated-deps
theta    MISSING   1.0.1   1.0.1  -                   outdated-should-display-outdated-deps
`

exports[`test/lib/outdated.js TAP should display outdated deps outdated --json --long > must match snapshot 1`] = `

{
  "alpha": {
    "current": "1.0.0",
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "outdated-should-display-outdated-deps",
    "location": "{CWD}/test/lib/outdated-should-display-outdated-deps/node_modules/alpha",
    "type": "dependencies"
  },
  "beta": {
    "current": "1.0.0",
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "outdated-should-display-outdated-deps",
    "location": "{CWD}/test/lib/outdated-should-display-outdated-deps/node_modules/beta",
    "type": "peerDependencies"
  },
  "gamma": {
    "current": "1.0.1",
    "wanted": "1.0.1",
    "latest": "2.0.0",
    "dependent": "outdated-should-display-outdated-deps",
    "location": "{CWD}/test/lib/outdated-should-display-outdated-deps/node_modules/gamma",
    "type": "dependencies"
  },
  "theta": {
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "outdated-should-display-outdated-deps",
    "type": "dependencies"
  }
}
`

exports[`test/lib/outdated.js TAP should display outdated deps outdated --json > must match snapshot 1`] = `

{
  "alpha": {
    "current": "1.0.0",
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "outdated-should-display-outdated-deps",
    "location": "{CWD}/test/lib/outdated-should-display-outdated-deps/node_modules/alpha"
  },
  "beta": {
    "current": "1.0.0",
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "outdated-should-display-outdated-deps",
    "location": "{CWD}/test/lib/outdated-should-display-outdated-deps/node_modules/beta"
  },
  "gamma": {
    "current": "1.0.1",
    "wanted": "1.0.1",
    "latest": "2.0.0",
    "dependent": "outdated-should-display-outdated-deps",
    "location": "{CWD}/test/lib/outdated-should-display-outdated-deps/node_modules/gamma"
  },
  "theta": {
    "wanted": "1.0.1",
    "latest": "1.0.1",
    "dependent": "outdated-should-display-outdated-deps"
  }
}
`

exports[`test/lib/outdated.js TAP should display outdated deps outdated --long > must match snapshot 1`] = `

Package  Current  Wanted  Latest  Location            Depended by                            Package Type      Homepage
alpha      1.0.0   1.0.1   1.0.1  node_modules/alpha  outdated-should-display-outdated-deps  dependencies
beta       1.0.0   1.0.1   1.0.1  node_modules/beta   outdated-should-display-outdated-deps  peerDependencies
gamma      1.0.1   1.0.1   2.0.0  node_modules/gamma  outdated-should-display-outdated-deps  dependencies
theta    MISSING   1.0.1   1.0.1  -                   outdated-should-display-outdated-deps  dependencies
`

exports[`test/lib/outdated.js TAP should display outdated deps outdated --omit=dev --omit=peer > must match snapshot 1`] = `

[4mPackage[24m  [4mCurrent[24m  [4mWanted[24m  [4mLatest[24m  [4mLocation[24m            [4mDepended by[24m
[31malpha[39m      1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/alpha  outdated-should-display-outdated-deps
[33mgamma[39m      1.0.1   [32m1.0.1[39m   [35m2.0.0[39m  node_modules/gamma  outdated-should-display-outdated-deps
[31mtheta[39m    MISSING   [32m1.0.1[39m   [35m1.0.1[39m  -                   outdated-should-display-outdated-deps
`

exports[`test/lib/outdated.js TAP should display outdated deps outdated --omit=dev > must match snapshot 1`] = `

[4mPackage[24m  [4mCurrent[24m  [4mWanted[24m  [4mLatest[24m  [4mLocation[24m            [4mDepended by[24m
[31malpha[39m      1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/alpha  outdated-should-display-outdated-deps
[31mbeta[39m       1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/beta   outdated-should-display-outdated-deps
[33mgamma[39m      1.0.1   [32m1.0.1[39m   [35m2.0.0[39m  node_modules/gamma  outdated-should-display-outdated-deps
[31mtheta[39m    MISSING   [32m1.0.1[39m   [35m1.0.1[39m  -                   outdated-should-display-outdated-deps
`

exports[`test/lib/outdated.js TAP should display outdated deps outdated --omit=prod > must match snapshot 1`] = `

[4mPackage[24m  [4mCurrent[24m  [4mWanted[24m  [4mLatest[24m  [4mLocation[24m            [4mDepended by[24m
[31malpha[39m      1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/alpha  outdated-should-display-outdated-deps
[31mbeta[39m       1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/beta   outdated-should-display-outdated-deps
[33mgamma[39m      1.0.1   [32m1.0.1[39m   [35m2.0.0[39m  node_modules/gamma  outdated-should-display-outdated-deps
`

exports[`test/lib/outdated.js TAP should display outdated deps outdated --parseable --long > must match snapshot 1`] = `

{CWD}/test/lib/outdated-should-display-outdated-deps/node_modules/alpha:alpha@1.0.1:alpha@1.0.0:alpha@1.0.1:outdated-should-display-outdated-deps:dependencies:
{CWD}/test/lib/outdated-should-display-outdated-deps/node_modules/beta:beta@1.0.1:beta@1.0.0:beta@1.0.1:outdated-should-display-outdated-deps:peerDependencies:
{CWD}/test/lib/outdated-should-display-outdated-deps/node_modules/gamma:gamma@1.0.1:gamma@1.0.1:gamma@2.0.0:outdated-should-display-outdated-deps:dependencies:
:theta@1.0.1:MISSING:theta@1.0.1:outdated-should-display-outdated-deps:dependencies:
`

exports[`test/lib/outdated.js TAP should display outdated deps outdated --parseable > must match snapshot 1`] = `

{CWD}/test/lib/outdated-should-display-outdated-deps/node_modules/alpha:alpha@1.0.1:alpha@1.0.0:alpha@1.0.1:outdated-should-display-outdated-deps
{CWD}/test/lib/outdated-should-display-outdated-deps/node_modules/beta:beta@1.0.1:beta@1.0.0:beta@1.0.1:outdated-should-display-outdated-deps
{CWD}/test/lib/outdated-should-display-outdated-deps/node_modules/gamma:gamma@1.0.1:gamma@1.0.1:gamma@2.0.0:outdated-should-display-outdated-deps
:theta@1.0.1:MISSING:theta@1.0.1:outdated-should-display-outdated-deps
`

exports[`test/lib/outdated.js TAP should display outdated deps outdated > must match snapshot 1`] = `

[4mPackage[24m  [4mCurrent[24m  [4mWanted[24m  [4mLatest[24m  [4mLocation[24m            [4mDepended by[24m
[31malpha[39m      1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/alpha  outdated-should-display-outdated-deps
[31mbeta[39m       1.0.0   [32m1.0.1[39m   [35m1.0.1[39m  node_modules/beta   outdated-should-display-outdated-deps
[33mgamma[39m      1.0.1   [32m1.0.1[39m   [35m2.0.0[39m  node_modules/gamma  outdated-should-display-outdated-deps
[31mtheta[39m    MISSING   [32m1.0.1[39m   [35m1.0.1[39m  -                   outdated-should-display-outdated-deps
`

exports[`test/lib/outdated.js TAP should display outdated deps outdated global > must match snapshot 1`] = `

Package  Current  Wanted  Latest  Location            Depended by
alpha      1.0.0   1.0.1   1.0.1  node_modules/alpha  global
`

exports[`test/lib/outdated.js TAP should display outdated deps outdated specific dep > must match snapshot 1`] = `

Package  Current  Wanted  Latest  Location            Depended by
alpha      1.0.0   1.0.1   1.0.1  node_modules/alpha  outdated-should-display-outdated-deps
`
