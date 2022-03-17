/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/view.js TAP should log info by field name array field - 1 element > must match snapshot 1`] = `

claudia
`

exports[`test/lib/commands/view.js TAP should log info by field name array field - 2 elements > must match snapshot 1`] = `

maintainers[0].name = 'claudia'
maintainers[1].name = 'isaacs'
`

exports[`test/lib/commands/view.js TAP should log info by field name maintainers with email > must match snapshot 1`] = `

{
  "maintainers": [
    {
      "name": "claudia",
      "email": "c@yellow.com",
      "twitter": "cyellow"
    },
    {
      "name": "isaacs",
      "email": "i@yellow.com",
      "twitter": "iyellow"
    }
  ],
  "name": "yellow"
}
`

exports[`test/lib/commands/view.js TAP should log info by field name maintainers with url > must match snapshot 1`] = `

[
  "claudia (http://c.pink.com)",
  "isaacs (http://i.pink.com)"
]
`

exports[`test/lib/commands/view.js TAP should log info by field name nested field with brackets > must match snapshot 1`] = `

"123"
`

exports[`test/lib/commands/view.js TAP should log info by field name readme > must match snapshot 1`] = `

a very useful readme
`

exports[`test/lib/commands/view.js TAP should log info by field name several fields > must match snapshot 1`] = `

{
  "name": "yellow",
  "version": "1.0.0"
}
`

exports[`test/lib/commands/view.js TAP should log info by field name several fields with several versions > must match snapshot 1`] = `

yellow@1.0.0 'claudia'
yellow@1.0.1 'claudia'
yellow@1.0.2 'claudia'
`

exports[`test/lib/commands/view.js TAP should log info of package in current working dir directory > must match snapshot 1`] = `


[4m[1m[32mblue[39m@[32m1.0.0[39m[22m[24m | [1m[31mProprietary[39m[22m | deps: [32mnone[39m | versions: [33m2[39m

dist
.tarball:[36mhttp://hm.blue.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0

published {TIME} ago[39m
`

exports[`test/lib/commands/view.js TAP should log info of package in current working dir non-specific version > must match snapshot 1`] = `


[4m[1m[32mblue[39m@[32m1.0.0[39m[22m[24m | [1m[31mProprietary[39m[22m | deps: [32mnone[39m | versions: [33m2[39m

dist
.tarball:[36mhttp://hm.blue.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0

published {TIME} ago[39m
`

exports[`test/lib/commands/view.js TAP should log info of package in current working dir specific version > must match snapshot 1`] = `


[4m[1m[32mblue[39m@[32m1.0.0[39m[22m[24m | [1m[31mProprietary[39m[22m | deps: [32mnone[39m | versions: [33m2[39m

dist
.tarball:[36mhttp://hm.blue.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0

published {TIME} ago[39m
`

exports[`test/lib/commands/view.js TAP should log package info package from git > must match snapshot 1`] = `


[4m[1m[32mgreen[39m@[32m1.0.0[39m[22m[24m | [32mACME[39m | deps: [36m2[39m | versions: [33m2[39m
green is a very important color

[1m[31mDEPRECATED[39m[22m!! - true

keywords:[33mcolors[39m, [33mgreen[39m, [33mcrayola[39m

bin:[33mgreen[39m

dist
.tarball:[36mhttp://hm.green.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dependencies:
[33mred[39m: 1.0.0
[33myellow[39m: 1.0.0

maintainers:
-[33mclaudia[39m <[36mc@yellow.com[39m>
-[33misaacs[39m <[36mi@yellow.com[39m>

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0
`

exports[`test/lib/commands/view.js TAP should log package info package with --json and semver range > must match snapshot 1`] = `

[
  {
    "_npmUser": "claudia <claudia@cyan.com>",
    "name": "cyan",
    "dist-tags": {
      "latest": "1.0.0"
    },
    "versions": [
      "1.0.0",
      "1.0.1"
    ],
    "version": "1.0.0",
    "dist": {
      "shasum": "123",
      "tarball": "http://hm.cyan.com/1.0.0.tgz",
      "integrity": "---",
      "fileCount": 1,
      "unpackedSize": 1
    }
  },
  {
    "_npmUser": "claudia <claudia@cyan.com>",
    "name": "cyan",
    "dist-tags": {
      "latest": "1.0.0"
    },
    "versions": [
      "1.0.0",
      "1.0.1"
    ]
  }
]
`

exports[`test/lib/commands/view.js TAP should log package info package with homepage > must match snapshot 1`] = `


[4m[1m[32morange[39m@[32m1.0.0[39m[22m[24m | [1m[31mProprietary[39m[22m | deps: [32mnone[39m | versions: [33m2[39m
[36mhttp://hm.orange.com[39m

dist
.tarball:[36mhttp://hm.orange.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0
`

exports[`test/lib/commands/view.js TAP should log package info package with license, bugs, repository and other fields > must match snapshot 1`] = `


[4m[1m[32mgreen[39m@[32m1.0.0[39m[22m[24m | [32mACME[39m | deps: [36m2[39m | versions: [33m2[39m
green is a very important color

[1m[31mDEPRECATED[39m[22m!! - true

keywords:[33mcolors[39m, [33mgreen[39m, [33mcrayola[39m

bin:[33mgreen[39m

dist
.tarball:[36mhttp://hm.green.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dependencies:
[33mred[39m: 1.0.0
[33myellow[39m: 1.0.0

maintainers:
-[33mclaudia[39m <[36mc@yellow.com[39m>
-[33misaacs[39m <[36mi@yellow.com[39m>

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0
`

exports[`test/lib/commands/view.js TAP should log package info package with maintainers info as object > must match snapshot 1`] = `


[4m[1m[32mpink[39m@[32m1.0.0[39m[22m[24m | [1m[31mProprietary[39m[22m | deps: [32mnone[39m | versions: [33m2[39m

dist
.tarball:[36mhttp://hm.pink.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0
`

exports[`test/lib/commands/view.js TAP should log package info package with more than 25 deps > must match snapshot 1`] = `


[4m[1m[32mblack[39m@[32m1.0.0[39m[22m[24m | [1m[31mProprietary[39m[22m | deps: [36m25[39m | versions: [33m2[39m

dist
.tarball:[36mhttp://hm.black.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dependencies:
[33m0[39m: 1.0.0
[33m10[39m: 1.0.0
[33m11[39m: 1.0.0
[33m12[39m: 1.0.0
[33m13[39m: 1.0.0
[33m14[39m: 1.0.0
[33m15[39m: 1.0.0
[33m16[39m: 1.0.0
[33m17[39m: 1.0.0
[33m18[39m: 1.0.0
[33m19[39m: 1.0.0
[33m1[39m: 1.0.0
[33m20[39m: 1.0.0
[33m21[39m: 1.0.0
[33m22[39m: 1.0.0
[33m23[39m: 1.0.0
[33m2[39m: 1.0.0
[33m3[39m: 1.0.0
[33m4[39m: 1.0.0
[33m5[39m: 1.0.0
[33m6[39m: 1.0.0
[33m7[39m: 1.0.0
[33m8[39m: 1.0.0
[33m9[39m: 1.0.0
(...and 1 more.)

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0
`

exports[`test/lib/commands/view.js TAP should log package info package with no modified time > must match snapshot 1`] = `


[4m[1m[32mcyan[39m@[32m1.0.0[39m[22m[24m | [1m[31mProprietary[39m[22m | deps: [32mnone[39m | versions: [33m2[39m

dist
.tarball:[36mhttp://hm.cyan.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0

published by [33mclaudia[39m <[36mclaudia@cyan.com[39m>
`

exports[`test/lib/commands/view.js TAP should log package info package with no repo or homepage > must match snapshot 1`] = `


[4m[1m[32mblue[39m@[32m1.0.0[39m[22m[24m | [1m[31mProprietary[39m[22m | deps: [32mnone[39m | versions: [33m2[39m

dist
.tarball:[36mhttp://hm.blue.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0

published {TIME} ago[39m
`

exports[`test/lib/commands/view.js TAP should log package info package with semver range > must match snapshot 1`] = `


[4m[1m[32mblue[39m@[32m1.0.0[39m[22m[24m | [1m[31mProprietary[39m[22m | deps: [32mnone[39m | versions: [33m2[39m

dist
.tarball:[36mhttp://hm.blue.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0

published {TIME} ago[39m

[4m[1m[32mblue[39m@[32m1.0.1[39m[22m[24m | [1m[31mProprietary[39m[22m | deps: [32mnone[39m | versions: [33m2[39m

dist
.tarball:[36mhttp://hm.blue.com/1.0.1.tgz[39m
.shasum:[33m124[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0

published [33mover a year from now[39m
`

exports[`test/lib/commands/view.js TAP workspaces all workspaces --json > must match snapshot 1`] = `

{
  "green": {
    "_id": "green",
    "name": "green",
    "dist-tags": {
      "latest": "1.0.0"
    },
    "maintainers": [
      {
        "name": "claudia",
        "email": "c@yellow.com",
        "twitter": "cyellow"
      },
      {
        "name": "isaacs",
        "email": "i@yellow.com",
        "twitter": "iyellow"
      }
    ],
    "keywords": [
      "colors",
      "green",
      "crayola"
    ],
    "versions": [
      "1.0.0",
      "1.0.1"
    ],
    "version": "1.0.0",
    "description": "green is a very important color",
    "bugs": {
      "url": "http://bugs.green.com"
    },
    "deprecated": true,
    "repository": {
      "url": "http://repository.green.com"
    },
    "license": {
      "type": "ACME"
    },
    "bin": {
      "green": "bin/green.js"
    },
    "dependencies": {
      "red": "1.0.0",
      "yellow": "1.0.0"
    },
    "dist": {
      "shasum": "123",
      "tarball": "http://hm.green.com/1.0.0.tgz",
      "integrity": "---",
      "fileCount": 1,
      "unpackedSize": 1
    }
  },
  "orange": {
    "name": "orange",
    "dist-tags": {
      "latest": "1.0.0"
    },
    "versions": [
      "1.0.0",
      "1.0.1"
    ],
    "version": "1.0.0",
    "homepage": "http://hm.orange.com",
    "license": {},
    "dist": {
      "shasum": "123",
      "tarball": "http://hm.orange.com/1.0.0.tgz",
      "integrity": "---",
      "fileCount": 1,
      "unpackedSize": 1
    }
  }
}
`

exports[`test/lib/commands/view.js TAP workspaces all workspaces > must match snapshot 1`] = `


[4m[1m[32mgreen[39m@[32m1.0.0[39m[22m[24m | [32mACME[39m | deps: [36m2[39m | versions: [33m2[39m
green is a very important color

[1m[31mDEPRECATED[39m[22m!! - true

keywords:[33mcolors[39m, [33mgreen[39m, [33mcrayola[39m

bin:[33mgreen[39m

dist
.tarball:[36mhttp://hm.green.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dependencies:
[33mred[39m: 1.0.0
[33myellow[39m: 1.0.0

maintainers:
-[33mclaudia[39m <[36mc@yellow.com[39m>
-[33misaacs[39m <[36mi@yellow.com[39m>

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0

[4m[1m[32morange[39m@[32m1.0.0[39m[22m[24m | [1m[31mProprietary[39m[22m | deps: [32mnone[39m | versions: [33m2[39m
[36mhttp://hm.orange.com[39m

dist
.tarball:[36mhttp://hm.orange.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0
`

exports[`test/lib/commands/view.js TAP workspaces all workspaces nonexistent field --json > must match snapshot 1`] = `

`

exports[`test/lib/commands/view.js TAP workspaces all workspaces nonexistent field > must match snapshot 1`] = `

green:
orange:
`

exports[`test/lib/commands/view.js TAP workspaces all workspaces single field --json > must match snapshot 1`] = `

{
  "green": "green",
  "orange": "orange"
}
`

exports[`test/lib/commands/view.js TAP workspaces all workspaces single field > must match snapshot 1`] = `

green:
green
orange:
orange
`

exports[`test/lib/commands/view.js TAP workspaces one specific workspace > must match snapshot 1`] = `


[4m[1m[32mgreen[39m@[32m1.0.0[39m[22m[24m | [32mACME[39m | deps: [36m2[39m | versions: [33m2[39m
green is a very important color

[1m[31mDEPRECATED[39m[22m!! - true

keywords:[33mcolors[39m, [33mgreen[39m, [33mcrayola[39m

bin:[33mgreen[39m

dist
.tarball:[36mhttp://hm.green.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dependencies:
[33mred[39m: 1.0.0
[33myellow[39m: 1.0.0

maintainers:
-[33mclaudia[39m <[36mc@yellow.com[39m>
-[33misaacs[39m <[36mi@yellow.com[39m>

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0
`

exports[`test/lib/commands/view.js TAP workspaces remote package name > must match snapshot 1`] = `
Ignoring workspaces for specified package(s)
`

exports[`test/lib/commands/view.js TAP workspaces remote package name > must match snapshot 2`] = `


[4m[1m[32mpink[39m@[32m1.0.0[39m[22m[24m | [1m[31mProprietary[39m[22m | deps: [32mnone[39m | versions: [33m2[39m

dist
.tarball:[36mhttp://hm.pink.com/1.0.0.tgz[39m
.shasum:[33m123[39m
.integrity:[33m---[39m
.unpackedSize:[33m1 B[39m

dist-tags:
[1m[32mlatest[39m[22m: 1.0.0
`

exports[`test/lib/commands/view.js TAP workspaces single workspace --json > must match snapshot 1`] = `

{
  "green": {
    "_id": "green",
    "name": "green",
    "dist-tags": {
      "latest": "1.0.0"
    },
    "maintainers": [
      {
        "name": "claudia",
        "email": "c@yellow.com",
        "twitter": "cyellow"
      },
      {
        "name": "isaacs",
        "email": "i@yellow.com",
        "twitter": "iyellow"
      }
    ],
    "keywords": [
      "colors",
      "green",
      "crayola"
    ],
    "versions": [
      "1.0.0",
      "1.0.1"
    ],
    "version": "1.0.0",
    "description": "green is a very important color",
    "bugs": {
      "url": "http://bugs.green.com"
    },
    "deprecated": true,
    "repository": {
      "url": "http://repository.green.com"
    },
    "license": {
      "type": "ACME"
    },
    "bin": {
      "green": "bin/green.js"
    },
    "dependencies": {
      "red": "1.0.0",
      "yellow": "1.0.0"
    },
    "dist": {
      "shasum": "123",
      "tarball": "http://hm.green.com/1.0.0.tgz",
      "integrity": "---",
      "fileCount": 1,
      "unpackedSize": 1
    }
  }
}
`
