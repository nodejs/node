'use strict'
var updatePackageJson = require('../update-package-json')
var npm = require('../../npm.js')
var packageId = require('../../utils/package-id.js')
var cache = require('../../cache.js')

module.exports = function (top, buildpath, pkg, log, next) {
  log.silly('extract', packageId(pkg))
  var up = npm.config.get('unsafe-perm')
  var user = up ? null : npm.config.get('user')
  var group = up ? null : npm.config.get('group')
  cache.unpack(pkg.package.name, pkg.package.version
        , buildpath
        , null, null, user, group,
        function (er) {
          if (er) return next(er)
          updatePackageJson(pkg, buildpath, next)
        })
}
