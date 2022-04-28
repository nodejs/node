/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/publish.js TAP _auth config default registry > new package version 1`] = `
+ test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP bare _auth and registry config > new package version 1`] = `
+ @npm/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP dry-run > must match snapshot 1`] = `
Array [
  Array [
    "",
  ],
  Array [
    "",
    "package: test-package@1.0.0",
  ],
  Array [
    "=== Tarball Contents ===",
  ],
  Array [
    "",
    "87B package.json",
  ],
  Array [
    "=== Tarball Details ===",
  ],
  Array [
    "",
    String(
      name:          test-package
      version:       1.0.0
      filename:      test-package-1.0.0.tgz
      package size:  160 B
      unpacked size: 87 B
      shasum:{sha}
      integrity:{sha}
      total files:   1
    ),
  ],
  Array [
    "",
    "",
  ],
  Array [
    "",
    "Publishing to https://registry.npmjs.org/ (dry-run)",
  ],
]
`

exports[`test/lib/commands/publish.js TAP has auth for scope configured registry > new package version 1`] = `
+ @npm/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP ignore-scripts > new package version 1`] = `
+ test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP json > must match snapshot 1`] = `
Array [
  Array [
    "",
    "Publishing to https://registry.npmjs.org/",
  ],
]
`

exports[`test/lib/commands/publish.js TAP json > new package json 1`] = `
{
  "id": "test-package@1.0.0",
  "name": "test-package",
  "version": "1.0.0",
  "size": 160,
  "unpackedSize": 87,
  "shasum": "{sha}",
  "integrity": "{sha}",
  "filename": "test-package-1.0.0.tgz",
  "files": [
    {
      "path": "package.json",
      "size": 87,
      "mode": 420
    }
  ],
  "entryCount": 1,
  "bundled": []
}
`

exports[`test/lib/commands/publish.js TAP no auth dry-run > must match snapshot 1`] = `
+ test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP no auth dry-run > warns about auth being needed 1`] = `
Array [
  Array [
    "",
    "This command requires you to be logged in to https://registry.npmjs.org/ (dry-run)",
  ],
]
`

exports[`test/lib/commands/publish.js TAP re-loads publishConfig.registry if added during script process > new package version 1`] = `
+ test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP respects publishConfig.registry, runs appropriate scripts > new package version 1`] = `

`

exports[`test/lib/commands/publish.js TAP scoped _auth config scoped registry > new package version 1`] = `
+ @npm/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP tarball > must match snapshot 1`] = `
Array [
  Array [
    "",
  ],
  Array [
    "",
    "package: test-tar-package@1.0.0",
  ],
  Array [
    "=== Tarball Contents ===",
  ],
  Array [
    "",
    String(
      26B index.js
      98B package.json
    ),
  ],
  Array [
    "=== Tarball Details ===",
  ],
  Array [
    "",
    String(
      name:          test-tar-package
      version:       1.0.0
      filename:      test-tar-package-1.0.0.tgz
      package size:  218 B
      unpacked size: 124 B
      shasum:{sha}
      integrity:{sha}
      total files:   2
    ),
  ],
  Array [
    "",
    "",
  ],
  Array [
    "",
    "Publishing to https://registry.npmjs.org/",
  ],
]
`

exports[`test/lib/commands/publish.js TAP tarball > new package json 1`] = `
+ test-tar-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP workspaces all workspaces - color > all public workspaces 1`] = `
+ workspace-a@1.2.3-a
+ workspace-b@1.2.3-n
+ workspace-n@1.2.3-n
`

exports[`test/lib/commands/publish.js TAP workspaces all workspaces - color > warns about skipped private workspace in color 1`] = `
Array [
  Array [
    "publish",
    "Skipping workspace \\u001b[32mworkspace-p\\u001b[39m, marked as \\u001b[1mprivate\\u001b[22m",
  ],
]
`

exports[`test/lib/commands/publish.js TAP workspaces all workspaces - no color > all public workspaces 1`] = `
+ workspace-a@1.2.3-a
+ workspace-b@1.2.3-n
+ workspace-n@1.2.3-n
`

exports[`test/lib/commands/publish.js TAP workspaces all workspaces - no color > warns about skipped private workspace 1`] = `
Array [
  Array [
    "publish",
    "Skipping workspace workspace-p, marked as private",
  ],
]
`

exports[`test/lib/commands/publish.js TAP workspaces json > all workspaces in json 1`] = `
{
  "workspace-a": {
    "id": "workspace-a@1.2.3-a",
    "name": "workspace-a",
    "version": "1.2.3-a",
    "size": 162,
    "unpackedSize": 82,
    "shasum": "{sha}",
    "integrity": "{sha}",
    "filename": "workspace-a-1.2.3-a.tgz",
    "files": [
      {
        "path": "package.json",
        "size": 82,
        "mode": 420
      }
    ],
    "entryCount": 1,
    "bundled": []
  },
  "workspace-b": {
    "id": "workspace-b@1.2.3-n",
    "name": "workspace-b",
    "version": "1.2.3-n",
    "size": 171,
    "unpackedSize": 92,
    "shasum": "{sha}",
    "integrity": "{sha}",
    "filename": "workspace-b-1.2.3-n.tgz",
    "files": [
      {
        "path": "package.json",
        "size": 92,
        "mode": 420
      }
    ],
    "entryCount": 1,
    "bundled": []
  },
  "workspace-n": {
    "id": "workspace-n@1.2.3-n",
    "name": "workspace-n",
    "version": "1.2.3-n",
    "size": 140,
    "unpackedSize": 42,
    "shasum": "{sha}",
    "integrity": "{sha}",
    "filename": "workspace-n-1.2.3-n.tgz",
    "files": [
      {
        "path": "package.json",
        "size": 42,
        "mode": 420
      }
    ],
    "entryCount": 1,
    "bundled": []
  }
}
`

exports[`test/lib/commands/publish.js TAP workspaces one workspace - success > single workspace 1`] = `
+ workspace-a@1.2.3-a
`
