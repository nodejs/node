/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/publish.js TAP private workspaces colorless > should output all publishes 1`] = `
Array [
  "+ @npmcli/b@1.0.0",
]
`

exports[`test/lib/publish.js TAP private workspaces colorless > should publish all non-private workspaces 1`] = `
Array [
  Object {
    "_id": "@npmcli/b@1.0.0",
    "name": "@npmcli/b",
    "readme": "ERROR: No README data found!",
    "version": "1.0.0",
  },
]
`

exports[`test/lib/publish.js TAP private workspaces with color > should output all publishes 1`] = `
Array [
  "+ @npmcli/b@1.0.0",
]
`

exports[`test/lib/publish.js TAP private workspaces with color > should publish all non-private workspaces 1`] = `
Array [
  Object {
    "_id": "@npmcli/b@1.0.0",
    "name": "@npmcli/b",
    "readme": "ERROR: No README data found!",
    "version": "1.0.0",
  },
]
`

exports[`test/lib/publish.js TAP shows usage with wrong set of arguments > should print usage 1`] = `
Error: 
Usage: npm publish

Publish a package

Usage:
npm publish [<folder>]

Options:
[--tag <tag>] [--access <restricted|public>] [--dry-run] [--otp <otp>]
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

Run "npm help publish" for more info {
  "code": "EUSAGE",
}
`

exports[`test/lib/publish.js TAP workspaces all workspaces > should output all publishes 1`] = `
Array [
  "+ workspace-a@1.2.3-a",
  "+ workspace-b@1.2.3-n",
  "+ workspace-n@1.2.3-n",
]
`

exports[`test/lib/publish.js TAP workspaces all workspaces > should publish all workspaces 1`] = `
Array [
  Object {
    "_id": "workspace-a@1.2.3-a",
    "name": "workspace-a",
    "readme": "ERROR: No README data found!",
    "repository": Object {
      "type": "git",
      "url": "http://repo.workspace-a/",
    },
    "version": "1.2.3-a",
  },
  Object {
    "_id": "workspace-b@1.2.3-n",
    "bugs": Object {
      "url": "https://github.com/npm/workspace-b/issues",
    },
    "homepage": "https://github.com/npm/workspace-b#readme",
    "name": "workspace-b",
    "readme": "ERROR: No README data found!",
    "repository": Object {
      "type": "git",
      "url": "git+https://github.com/npm/workspace-b.git",
    },
    "version": "1.2.3-n",
  },
  Object {
    "_id": "workspace-n@1.2.3-n",
    "name": "workspace-n",
    "readme": "ERROR: No README data found!",
    "version": "1.2.3-n",
  },
]
`

exports[`test/lib/publish.js TAP workspaces json > should output all publishes as json 1`] = `
Array [
  String(
    {
      "workspace-a": {
        "id": "workspace-a@1.2.3-a"
      },
      "workspace-b": {
        "id": "workspace-b@1.2.3-n"
      },
      "workspace-n": {
        "id": "workspace-n@1.2.3-n"
      }
    }
  ),
]
`

exports[`test/lib/publish.js TAP workspaces json > should publish all workspaces 1`] = `
Array [
  Object {
    "_id": "workspace-a@1.2.3-a",
    "name": "workspace-a",
    "readme": "ERROR: No README data found!",
    "repository": Object {
      "type": "git",
      "url": "http://repo.workspace-a/",
    },
    "version": "1.2.3-a",
  },
  Object {
    "_id": "workspace-b@1.2.3-n",
    "bugs": Object {
      "url": "https://github.com/npm/workspace-b/issues",
    },
    "homepage": "https://github.com/npm/workspace-b#readme",
    "name": "workspace-b",
    "readme": "ERROR: No README data found!",
    "repository": Object {
      "type": "git",
      "url": "git+https://github.com/npm/workspace-b.git",
    },
    "version": "1.2.3-n",
  },
  Object {
    "_id": "workspace-n@1.2.3-n",
    "name": "workspace-n",
    "readme": "ERROR: No README data found!",
    "version": "1.2.3-n",
  },
]
`

exports[`test/lib/publish.js TAP workspaces one workspace > should output one publish 1`] = `
Array [
  "+ workspace-a@1.2.3-a",
]
`

exports[`test/lib/publish.js TAP workspaces one workspace > should publish given workspace 1`] = `
Array [
  Object {
    "_id": "workspace-a@1.2.3-a",
    "name": "workspace-a",
    "readme": "ERROR: No README data found!",
    "repository": Object {
      "type": "git",
      "url": "http://repo.workspace-a/",
    },
    "version": "1.2.3-a",
  },
]
`
