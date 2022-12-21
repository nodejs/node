#!/usr/bin/env node
'use strict'
require('@iarna/cli')(main)
  .usage('lock-verify [projectPath]')
  .help()

const lockVerify = require('./index.js')

function main (opts, check) {
  return lockVerify(check).then(result => {
    result.warnings.forEach(w => console.error('Warning:', w))
    if (!result.status) {
      result.errors.forEach(e => console.error(e))
      throw 1
    }
  })
}
