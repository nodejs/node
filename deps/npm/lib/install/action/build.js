'use strict'
var chain = require('slide').chain
var build = require('../../build.js')
var npm = require('../../npm.js')
var packageId = require('../../utils/package-id.js')

module.exports = function (staging, pkg, log, next) {
  log.silly('build', packageId(pkg))
  chain([
    [build.linkStuff, pkg.package, pkg.path, npm.config.get('global')],
    [build.writeBuiltinConf, pkg.package, pkg.path]
  ], next)
}
