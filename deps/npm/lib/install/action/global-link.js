'use strict'
var npm = require('../../npm.js')

module.exports = function (top, buildpath, pkg, log, next) {
  log.silly('global-link', pkg.package.name)
  npm.link(pkg.package.name, next)
}
