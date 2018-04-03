'use strict'
var lifecycle = require('../../utils/lifecycle.js')
var packageId = require('../../utils/package-id.js')

module.exports = function (staging, pkg, log, next) {
  log.silly('install', packageId(pkg))
  lifecycle(pkg.package, 'install', pkg.path, next)
}
