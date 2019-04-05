'use strict'
var moduleName = require('../../utils/module-name.js')
var npm = require('../../npm.js')
var packageId = require('../../utils/package-id.js')

module.exports = function (staging, pkg, log, next) {
  log.silly('global-link', packageId(pkg))
  npm.link(moduleName(pkg), next)
}
