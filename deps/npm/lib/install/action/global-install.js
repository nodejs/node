'use strict'
var path = require('path')
var npm = require('../../npm.js')
var Installer = require('../../install.js').Installer
var packageId = require('../../utils/package-id.js')
var moduleName = require('../../utils/module-name.js')

module.exports = function (staging, pkg, log, next) {
  log.silly('global-install', packageId(pkg))
  var globalRoot = path.resolve(npm.globalDir, '..')
  npm.config.set('global', true)
  var install = new Installer(globalRoot, false, [moduleName(pkg) + '@' + pkg.package._requested.rawSpec])
  install.link = false
  install.run(function () {
    npm.config.set('global', false)
    next.apply(null, arguments)
  })
}
