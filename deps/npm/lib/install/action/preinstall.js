'use strict'
var lifecycle = require('../../utils/lifecycle.js')
var packageId = require('../../utils/package-id.js')
var moduleStagingPath = require('../module-staging-path.js')

module.exports = function (staging, pkg, log, next) {
  log.silly('preinstall', packageId(pkg))
  lifecycle(pkg.package, 'preinstall', moduleStagingPath(staging, pkg), false, false, next)
}
