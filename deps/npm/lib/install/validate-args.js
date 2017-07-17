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
      [hasMinimumFields, pkg],
      [checkSelf, idealTree, pkg, force],
      [isInstallable, pkg]
    ], done)
  }, next)
}

function hasMinimumFields (pkg, cb) {
  if (pkg.name === '' || pkg.name == null) {
    return cb(new Error(`Can't install ${pkg._resolved}: Missing package name`))
  } else if (pkg.version === '' || pkg.version == null) {
    return cb(new Error(`Can't install ${pkg._resolved}: Missing package version`))
  } else {
    return cb()
  }
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
    var er = new Error('Refusing to install package with name "' + pkg.name +
     '" under a package\n' +
     'also called "' + pkg.name + '". Did you name your project the same\n' +
     'as the dependency you\'re installing?\n\n' +
     'For more information, see:\n' +
     '    <https://docs.npmjs.com/cli/install#limitations-of-npms-install-algorithm>')
    er.code = 'ENOSELF'
    next(er)
  }
}
