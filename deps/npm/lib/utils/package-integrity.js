'use strict'

// Utilities for generating and verifying the packageIntegrity field for
// package-lock
//
// Spec: https://github.com/npm/npm/pull/16441

const ssri = require('ssri')
const SSRI_OPTS = {
  algorithms: ['sha512']
}

module.exports.check = check
function check (pkg, integrity) {
  return ssri.checkData(JSON.stringify(pkg), integrity, SSRI_OPTS)
}

module.exports.hash = hash
function hash (pkg) {
  return ssri.fromData(JSON.stringify(pkg), SSRI_OPTS).toString()
}
