/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/pack.js TAP dry run > logs pack contents 1`] = `
Array [
  undefined,
  "package: test-package@1.0.0",
  undefined,
  "41B package.json",
  undefined,
  String(
    name:          test-package
    version:       1.0.0
    filename:      test-package-1.0.0.tgz
    package size:  136 B
    unpacked size: 41 B
    shasum:        a92a0679a70a450f14f98a468756948a679e4107
    integrity:     sha512-Gka9ZV/Bryxky[...]LgMJ+0F+FhXMA==
    total files:   1
  ),
  "",
]
`

exports[`test/lib/commands/pack.js TAP should log output as valid json > logs pack contents 1`] = `
Array []
`

exports[`test/lib/commands/pack.js TAP should log output as valid json > outputs as json 1`] = `
Array [
  Array [
    Object {
      "bundled": Array [],
      "entryCount": 1,
      "filename": "test-package-1.0.0.tgz",
      "files": Array [
        Object {
          "mode": 420,
          "path": "package.json",
          "size": 41,
        },
      ],
      "id": "test-package@1.0.0",
      "integrity": "sha512-Gka9ZV/BryxkypfvMpTvLfaJE1AUi7PK1EAbYqnVzqtucf6QvUK4CFsLVzagY1GwZVx2T1jwWLgMJ+0F+FhXMA==",
      "name": "test-package",
      "shasum": "a92a0679a70a450f14f98a468756948a679e4107",
      "size": 136,
      "unpackedSize": 41,
      "version": "1.0.0",
    },
  ],
]
`

exports[`test/lib/commands/pack.js TAP should log scoped package output as valid json > logs pack contents 1`] = `
Array []
`

exports[`test/lib/commands/pack.js TAP should log scoped package output as valid json > outputs as json 1`] = `
Array [
  Array [
    Object {
      "bundled": Array [],
      "entryCount": 1,
      "filename": "myscope-test-package-1.0.0.tgz",
      "files": Array [
        Object {
          "mode": 420,
          "path": "package.json",
          "size": 50,
        },
      ],
      "id": "@myscope/test-package@1.0.0",
      "integrity": "sha512-bUu8iTm2E5DZMrwKeyx963K6ViEmaFocXh75EujgI+FHSaJeqvObcdk1KFwdx8CbOgsfNHEvWNQw/bONAJsoNw==",
      "name": "@myscope/test-package",
      "shasum": "7e6eb2e1ca46bed6b8fa8e144e0fcd1b22fe2d98",
      "size": 145,
      "unpackedSize": 50,
      "version": "1.0.0",
    },
  ],
]
`

exports[`test/lib/commands/pack.js TAP should pack current directory with no arguments > logs pack contents 1`] = `
Array [
  undefined,
  "package: test-package@1.0.0",
  undefined,
  "41B package.json",
  undefined,
  String(
    name:          test-package
    version:       1.0.0
    filename:      test-package-1.0.0.tgz
    package size:  136 B
    unpacked size: 41 B
    shasum:        a92a0679a70a450f14f98a468756948a679e4107
    integrity:     sha512-Gka9ZV/Bryxky[...]LgMJ+0F+FhXMA==
    total files:   1
  ),
  "",
]
`
