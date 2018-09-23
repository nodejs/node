'use strict'
var validate = require('aproba')
var asyncMap = require('slide').asyncMap
var chain = require('slide').chain
var npmInstallChecks = require('npm-install-checks')
var checkEngine = npmInstallChecks.checkEngine
var checkPlatform = npmInstallChecks.checkPlatform
var npm = require('../npm.js')

module.exports = function (idealTree, args, next) {
  validate('OAF', arguments)
  var force = npm.config.get('force')

  asyncMap(args, function (pkg, done) {
    chain([
      [checkSelf, idealTree, pkg, force],
      [isInstallable, pkg]
    ], done)
  }, next)
}

var isInstallable = module.exports.isInstallable = function (pkg, next) {
  var force = npm.config.get('force')
  var nodeVersion = npm.config.get('node-version')
  var strict = npm.config.get('engine-strict')
  chain([
    [checkEngine, pkg, npm.version, nodeVersion, force, strict],
    [checkPlatform, pkg, force]
  ], next)
}

function checkSelf (idealTree, pkg, force, next) {
  if (idealTree.package && idealTree.package.name !== pkg.name) return next()
  if (force) {
    var warn = new Error("Wouldn't install " + pkg.name + ' as a dependency of itself, but being forced')
    warn.code = 'ENOSELF'
    idealTree.warnings.push(warn)
    next()
  } else {
    var er = new Error('Refusing to install ' + pkg.name + ' as a dependency of itself')
    er.code = 'ENOSELF'
    next(er)
  }
}
