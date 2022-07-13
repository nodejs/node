/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/init.js TAP npm init workspces with root > does not print helper info 1`] = `
Array []
`

exports[`test/lib/commands/init.js TAP workspaces no args > should print helper info 1`] = `
Array []
`

exports[`test/lib/commands/init.js TAP workspaces no args, existing folder > should print helper info 1`] = `
Array []
`

exports[`test/lib/commands/init.js TAP workspaces post workspace-init reify > should print helper info 1`] = `
Array [
  Array [
    String(

      added 1 package in 100ms
    ),
  ],
]
`

exports[`test/lib/commands/init.js TAP workspaces post workspace-init reify > should reify tree on init ws complete 1`] = `
{
  "name": "top-level",
  "lockfileVersion": 2,
  "requires": true,
  "packages": {
    "": {
      "name": "top-level",
      "workspaces": [
        "a"
      ]
    },
    "a": {
      "version": "1.0.0",
      "license": "ISC",
      "devDependencies": {}
    },
    "node_modules/a": {
      "resolved": "a",
      "link": true
    }
  },
  "dependencies": {
    "a": {
      "version": "file:a"
    }
  }
}

`

exports[`test/lib/commands/init.js TAP workspaces with arg but missing workspace folder > should print helper info 1`] = `
Array []
`
