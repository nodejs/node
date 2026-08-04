/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/utils/validate-lockfile.js TAP extra inventory items on idealTree > should have missing entries error 1`] = `
Array [
  "Missing: baz@3.0.0 from lock file",
]
`

exports[`test/lib/utils/validate-lockfile.js TAP extra inventory items on virtualTree > should have no errors if finding virtualTree extra items 1`] = `
Array []
`

exports[`test/lib/utils/validate-lockfile.js TAP identical inventory for both idealTree and virtualTree > should have no errors on identical inventories 1`] = `
Array []
`

exports[`test/lib/utils/validate-lockfile.js TAP mismatching versions on inventory > should have errors for each mismatching version 1`] = `
Array [
  "Invalid: lock file's foo@1.0.0 does not satisfy foo@2.0.0",
  "Invalid: lock file's bar@2.0.0 does not satisfy bar@3.0.0",
]
`

exports[`test/lib/utils/validate-lockfile.js TAP missing virtualTree inventory > should have errors for each mismatching version 1`] = `
Array [
  "Missing: foo@1.0.0 from lock file",
  "Missing: bar@2.0.0 from lock file",
  "Missing: baz@3.0.0 from lock file",
]
`
