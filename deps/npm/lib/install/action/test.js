'use strict'
var lifecycle = require('../../utils/lifecycle.js')
var packageId = require('../../utils/package-id.js')

module.exports = function (top, buildpath, pkg, log, next) {
  log.silly('test', packageId(pkg), buildpath)
  lifecycle(pkg.package, 'test', buildpath, false, false, next)
}
