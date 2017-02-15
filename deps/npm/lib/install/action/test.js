'use strict'
var lifecycle = require('../../utils/lifecycle.js')
var packageId = require('../../utils/package-id.js')

module.exports = function (staging, pkg, log, next) {
  log.silly('test', packageId(pkg))
  lifecycle(pkg.package, 'test', pkg.path, false, false, next)
}
