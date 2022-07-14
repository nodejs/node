/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/version.js TAP empty versions workspaces with one arg, all workspaces > must match snapshot 1`] = `
{
  "name": "workspaces-test",
  "version": "1.0.0",
  "lockfileVersion": 2,
  "requires": true,
  "packages": {
    "": {
      "name": "workspaces-test",
      "version": "1.0.0",
      "workspaces": [
        "workspace-a",
        "workspace-b"
      ]
    },
    "node_modules/workspace-a": {
      "resolved": "workspace-a",
      "link": true
    },
    "node_modules/workspace-b": {
      "resolved": "workspace-b",
      "link": true
    },
    "workspace-a": {
      "version": "2.0.0"
    },
    "workspace-b": {
      "version": "2.0.0"
    }
  },
  "dependencies": {
    "workspace-a": {
      "version": "file:workspace-a"
    },
    "workspace-b": {
      "version": "file:workspace-b"
    }
  }
}

`

exports[`test/lib/commands/version.js TAP empty versions workspaces with one arg, all workspaces, saves package.json > must match snapshot 1`] = `
{
  "name": "workspaces-test",
  "version": "1.0.0",
  "lockfileVersion": 2,
  "requires": true,
  "packages": {
    "": {
      "name": "workspaces-test",
      "version": "1.0.0",
      "workspaces": [
        "workspace-a",
        "workspace-b"
      ],
      "dependencies": {
        "workspace-a": "^2.0.0",
        "workspace-b": "^2.0.0"
      }
    },
    "node_modules/workspace-a": {
      "resolved": "workspace-a",
      "link": true
    },
    "node_modules/workspace-b": {
      "resolved": "workspace-b",
      "link": true
    },
    "workspace-a": {
      "version": "2.0.0"
    },
    "workspace-b": {
      "version": "2.0.0"
    }
  },
  "dependencies": {
    "workspace-a": {
      "version": "file:workspace-a"
    },
    "workspace-b": {
      "version": "file:workspace-b"
    }
  }
}

`
