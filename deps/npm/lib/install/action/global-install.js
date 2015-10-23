'use strict'
var path = require('path')
var npm = require('../../npm.js')
var Installer = require('../../install.js').Installer

module.exports = function (top, buildpath, pkg, log, next) {
  log.silly('global-install', pkg.package.name)
  var globalRoot = path.resolve(npm.globalDir, '..')
  npm.config.set('global', true)
  var install = new Installer(globalRoot, false, [pkg.package.name + '@' + pkg.package._requested.spec])
  install.link = false
  install.run(function () {
    npm.config.set('global', false)
    next.apply(null, arguments)
  })
}
