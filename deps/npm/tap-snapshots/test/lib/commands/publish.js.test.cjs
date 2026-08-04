/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/publish.js TAP _auth config default registry > new package version 1`] = `
+ @npmcli/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP bare _auth and registry config > new package version 1`] = `
+ @npm/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP dry-run > must match snapshot 1`] = `
Array [
  "package: @npmcli/test-package@1.0.0",
  "Tarball Contents",
  "95B package.json",
  "Tarball Details",
  "name: @npmcli/test-package",
  "version: 1.0.0",
  "filename: npmcli-test-package-1.0.0.tgz",
  "package size: {size}",
  "unpacked size: 95 B",
  "shasum: {sha}",
  "integrity: {integrity}
  "total files: 1",
  "Publishing to https://registry.npmjs.org/ with tag latest and default access (dry-run)",
]
`

exports[`test/lib/commands/publish.js TAP foreground-scripts can still be set to false > must match snapshot 1`] = `
Array [
  "package: test-fg-scripts@0.0.0",
  "Tarball Contents",
  "110B package.json",
  "Tarball Details",
  "name: test-fg-scripts",
  "version: 0.0.0",
  "filename: test-fg-scripts-0.0.0.tgz",
  "package size: {size}",
  "unpacked size: 110 B",
  "shasum: {sha}",
  "integrity: {integrity}
  "total files: 1",
  "Publishing to https://registry.npmjs.org/ with tag latest and default access (dry-run)",
]
`

exports[`test/lib/commands/publish.js TAP foreground-scripts defaults to true > must match snapshot 1`] = `
Array [
  "run test-fg-scripts@0.0.0 prepack",
  "run echo prepack!",
  "run test-fg-scripts@0.0.0 postpack",
  "run echo postpack!",
  "package: test-fg-scripts@0.0.0",
  "Tarball Contents",
  "110B package.json",
  "Tarball Details",
  "name: test-fg-scripts",
  "version: 0.0.0",
  "filename: test-fg-scripts-0.0.0.tgz",
  "package size: {size}",
  "unpacked size: 110 B",
  "shasum: {sha}",
  "integrity: {integrity}
  "total files: 1",
  "Publishing to https://registry.npmjs.org/ with tag latest and default access (dry-run)",
]
`

exports[`test/lib/commands/publish.js TAP has mTLS auth for scope configured registry > new package version 1`] = `
+ @npm/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP has token auth for scope configured registry > new package version 1`] = `
+ @npm/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP ignore-scripts > new package version 1`] = `
+ @npmcli/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP json > must match snapshot 1`] = `
Array [
  "Publishing to https://registry.npmjs.org/ with tag latest and default access",
]
`

exports[`test/lib/commands/publish.js TAP json > new package json 1`] = `
{
  "@npmcli/test-package": {
    "id": "@npmcli/test-package@1.0.0",
    "name": "@npmcli/test-package",
    "version": "1.0.0",
    "size": "{size}",
    "unpackedSize": 95,
    "shasum": "{sha}",
    "integrity": "{integrity}",
    "filename": "npmcli-test-package-1.0.0.tgz",
    "files": [
      {
        "path": "package.json",
        "size": "{size}",
        "mode": 420
      }
    ],
    "entryCount": 1,
    "bundled": []
  }
}
`

exports[`test/lib/commands/publish.js TAP manifest > manifest 1`] = `
Object {
  "_id": "npm@{VERSION}",
  "author": Object {
    "name": "GitHub Inc.",
  },
  "bin": Object {
    "npm": "bin/npm-cli.js",
    "npx": "bin/npx-cli.js",
  },
  "bugs": Object {
    "url": "https://github.com/npm/cli/issues",
  },
  "description": "a package manager for JavaScript",
  "exports": Object {
    ".": Array [
      Object {
        "default": "./index.js",
      },
      "./index.js",
    ],
    "./package.json": "./package.json",
  },
  "files": Array [
    "bin/",
    "lib/",
    "index.js",
    "docs/content/",
    "docs/output/",
    "man/",
  ],
  "homepage": "https://docs.npmjs.com/",
  "keywords": Array [
    "install",
    "modules",
    "package manager",
    "package.json",
  ],
  "license": "Artistic-2.0",
  "main": "./index.js",
  "name": "npm",
  "readmeFilename": "README.md",
  "repository": Object {
    "type": "git",
    "url": "git+https://github.com/npm/cli.git",
  },
  "version": "{VERSION}",
}
`

exports[`test/lib/commands/publish.js TAP no auth dry-run > must match snapshot 1`] = `
+ @npmcli/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP no auth dry-run > warns about auth being needed 1`] = `
Array [
  "publish This command requires you to be logged in to https://registry.npmjs.org/ (dry-run)",
]
`

exports[`test/lib/commands/publish.js TAP prioritize CLI flags over publishConfig > new package version 1`] = `
+ @npmcli/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP private access > must match snapshot 1`] = `
Array [
  "package: @npm/test-package@1.0.0",
  "Tarball Contents",
  "55B package.json",
  "Tarball Details",
  "name: @npm/test-package",
  "version: 1.0.0",
  "filename: npm-test-package-1.0.0.tgz",
  "package size: {size}",
  "unpacked size: 55 B",
  "shasum: {sha}",
  "integrity: {integrity}
  "total files: 1",
  "Publishing to https://registry.npmjs.org/ with tag latest and restricted access",
]
`

exports[`test/lib/commands/publish.js TAP private access > new package version 1`] = `
+ @npm/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP public access > must match snapshot 1`] = `
Array [
  "package: @npm/test-package@1.0.0",
  "Tarball Contents",
  "55B package.json",
  "Tarball Details",
  "name: @npm/test-package",
  "version: 1.0.0",
  "filename: npm-test-package-1.0.0.tgz",
  "package size: {size}",
  "unpacked size: 55 B",
  "shasum: {sha}",
  "integrity: {integrity}
  "total files: 1",
  "Publishing to https://registry.npmjs.org/ with tag latest and public access",
]
`

exports[`test/lib/commands/publish.js TAP public access > new package version 1`] = `
+ @npm/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP re-loads publishConfig.registry if added during script process > new package version 1`] = `
+ @npmcli/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP respects publishConfig.registry, runs appropriate scripts > new package version 1`] = `
+ @npmcli/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP restricted access > must match snapshot 1`] = `
Array [
  "package: @npm/test-package@1.0.0",
  "Tarball Contents",
  "55B package.json",
  "Tarball Details",
  "name: @npm/test-package",
  "version: 1.0.0",
  "filename: npm-test-package-1.0.0.tgz",
  "package size: {size}",
  "unpacked size: 55 B",
  "shasum: {sha}",
  "integrity: {integrity}
  "total files: 1",
  "Publishing to https://registry.npmjs.org/ with tag latest and restricted access",
]
`

exports[`test/lib/commands/publish.js TAP restricted access > new package version 1`] = `
+ @npm/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP scoped _auth config scoped registry > new package version 1`] = `
+ @npm/test-package@1.0.0
`

exports[`test/lib/commands/publish.js TAP tarball > must match snapshot 1`] = `
Array [
  "package: test-tar-package@1.0.0",
  "Tarball Contents",
  String(
    26B index.js
    98B package.json
  ),
  "Tarball Details",
  "name: test-tar-package",
  "version: 1.0.0",
  "filename: test-tar-package-1.0.0.tgz",
  "package size: {size}",
  "unpacked size: 124 B",
  "shasum: {sha}",
  "integrity: {integrity}
  "total files: 2",
  "Publishing to https://registry.npmjs.org/ with tag latest and default access",
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
  "\\u001b[94mpublish\\u001b[39m npm auto-corrected some errors in your package.json when publishing.  Please run \\"npm pkg fix\\" to address these errors.",
  String(
    \\u001b[94mpublish\\u001b[39m errors corrected:
    \\u001b[94mpublish\\u001b[39m "repository" was changed from a string to an object
  ),
  "\\u001b[94mpublish\\u001b[39m npm auto-corrected some errors in your package.json when publishing.  Please run \\"npm pkg fix\\" to address these errors.",
  String(
    \\u001b[94mpublish\\u001b[39m errors corrected:
    \\u001b[94mpublish\\u001b[39m "repository" was changed from a string to an object
    \\u001b[94mpublish\\u001b[39m "repository.url" was normalized to "git+https://github.com/npm/workspace-b.git"
  ),
  "\\u001b[94mpublish\\u001b[39m Skipping workspace \\u001b[36mworkspace-p\\u001b[39m, marked as \\u001b[1mprivate\\u001b[22m",
]
`

exports[`test/lib/commands/publish.js TAP workspaces all workspaces - no color > all public workspaces 1`] = `
+ workspace-a@1.2.3-a
+ workspace-b@1.2.3-n
+ workspace-n@1.2.3-n
`

exports[`test/lib/commands/publish.js TAP workspaces all workspaces - no color > warns about skipped private workspace 1`] = `
Array [
  "publish npm auto-corrected some errors in your package.json when publishing.  Please run \\"npm pkg fix\\" to address these errors.",
  String(
    publish errors corrected:
    publish "repository" was changed from a string to an object
  ),
  "publish npm auto-corrected some errors in your package.json when publishing.  Please run \\"npm pkg fix\\" to address these errors.",
  String(
    publish errors corrected:
    publish "repository" was changed from a string to an object
    publish "repository.url" was normalized to "git+https://github.com/npm/workspace-b.git"
  ),
  "publish Skipping workspace workspace-p, marked as private",
]
`

exports[`test/lib/commands/publish.js TAP workspaces all workspaces - some marked private > one marked private 1`] = `
+ workspace-a@1.2.3-a
`

exports[`test/lib/commands/publish.js TAP workspaces different package spec > publish different package spec 1`] = `
+ pkg@1.2.3
`

exports[`test/lib/commands/publish.js TAP workspaces json > all workspaces in json 1`] = `
{
  "workspace-a": {
    "id": "workspace-a@1.2.3-a",
    "name": "workspace-a",
    "version": "1.2.3-a",
    "size": "{size}",
    "unpackedSize": 82,
    "shasum": "{sha}",
    "integrity": "{integrity}",
    "filename": "workspace-a-1.2.3-a.tgz",
    "files": [
      {
        "path": "package.json",
        "size": "{size}",
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
    "size": "{size}",
    "unpackedSize": 92,
    "shasum": "{sha}",
    "integrity": "{integrity}",
    "filename": "workspace-b-1.2.3-n.tgz",
    "files": [
      {
        "path": "package.json",
        "size": "{size}",
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
    "size": "{size}",
    "unpackedSize": 42,
    "shasum": "{sha}",
    "integrity": "{integrity}",
    "filename": "workspace-n-1.2.3-n.tgz",
    "files": [
      {
        "path": "package.json",
        "size": "{size}",
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
