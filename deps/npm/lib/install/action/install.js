'use strict'
var lifecycle = require('../../utils/lifecycle.js')
var packageId = require('../../utils/package-id.js')

module.exports = function (top, buildpath, pkg, log, next) {
  log.silly('install', packageId(pkg), buildpath)
  lifecycle(pkg.package, 'install', pkg.path, false, false, next)
}
