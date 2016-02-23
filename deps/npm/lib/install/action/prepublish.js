'use strict'
var lifecycle = require('../../utils/lifecycle.js')
var packageId = require('../../utils/package-id.js')

module.exports = function (top, buildpath, pkg, log, next) {
  log.silly('prepublish', packageId(pkg), buildpath)
  lifecycle(pkg.package, 'prepublish', buildpath, false, false, next)
}
