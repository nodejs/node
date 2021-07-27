'use strict'

const npm = require('../npm.js')
var packageId = require('../utils/package-id.js')
const log = require('npmlog')

module.exports = binLinksOpts

function binLinksOpts (pkg) {
  return {
    ignoreScripts: npm.config.get('ignore-scripts'),
    force: npm.config.get('force'),
    globalBin: npm.globalBin,
    globalDir: npm.globalDir,
    json: npm.config.get('json'),
    log: log,
    name: 'npm',
    parseable: npm.config.get('parseable'),
    pkgId: packageId(pkg),
    prefix: npm.config.get('prefix'),
    prefixes: [
      npm.prefix,
      npm.globalPrefix,
      npm.dir,
      npm.root,
      npm.globalDir,
      npm.bin,
      npm.globalBin
    ],
    umask: npm.config.get('umask')
  }
}
