'use strict'
var validate = require('aproba')
var asyncMap = require('slide').asyncMap
var chain = require('slide').chain
var npmInstallChecks = require('npm-install-checks')
var iferr = require('iferr')
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

function getWarnings (pkg) {
  while (pkg.parent) pkg = pkg.parent
  if (!pkg.warnings) pkg.warnings = []
  return pkg.warnings
}

var isInstallable = module.exports.isInstallable = function (pkg, next) {
  var force = npm.config.get('force')
  var nodeVersion = npm.config.get('node-version')
  if (/-/.test(nodeVersion)) {
    // for the purposes of validation, if the node version is a prerelease,
    // strip that. We check and warn about this sceanrio over in validate-tree.
    nodeVersion = nodeVersion.replace(/-.*/, '')
  }
  var strict = npm.config.get('engine-strict')
  checkEngine(pkg, npm.version, nodeVersion, force, strict, iferr(next, thenWarnEngineIssues))
  function thenWarnEngineIssues (warn) {
    if (warn) getWarnings(pkg).push(warn)
    checkPlatform(pkg, force, next)
  }
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
