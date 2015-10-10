'use strict'
var lifecycle = require('../../utils/lifecycle.js')

module.exports = function (top, buildpath, pkg, log, next) {
  log.silly('preinstall', pkg.package.name, buildpath)
  lifecycle(pkg.package, 'preinstall', buildpath, false, false, next)
}
