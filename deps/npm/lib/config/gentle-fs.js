'use strict'

const npm = require('../npm.js')
const log = require('npmlog')

module.exports = gentleFSOpts

function gentleFSOpts (gently, base, abs) {
  return {
    // never rm the root, prefix, or bin dirs
    //
    // globals included because of `npm link` -- as far as the package
    // requesting the link is concerned, the linked package is always
    // installed globally
    prefixes: [
      npm.prefix,
      npm.globalPrefix,
      npm.dir,
      npm.root,
      npm.globalDir,
      npm.bin,
      npm.globalBin
    ],
    absolute: abs,
    log: log,
    prefix: npm.prefix,
    force: npm.config.get('force'),
    gently: gently,
    base: base,
    name: 'npm'
  }
}
