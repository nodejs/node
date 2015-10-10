'use strict'
var lifecycle = require('../../utils/lifecycle.js')

module.exports = function (top, buildpath, pkg, log, next) {
  log.silly('install', pkg.package.name, buildpath)
  lifecycle(pkg.package, 'install', pkg.path, false, false, next)
}
