'use strict'
var npm = require('../../npm.js')
var packageId = require('../../utils/package-id.js')

module.exports = function (top, buildpath, pkg, log, next) {
  log.silly('global-link', packageId(pkg))
  npm.link(pkg.package.name, next)
}
