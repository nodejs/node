'use strict'
var path = require('path')
var validate = require('aproba')
var asyncMap = require('slide').asyncMap
var chain = require('slide').chain
var npmInstallChecks = require('npm-install-checks')
var checkGit = npmInstallChecks.checkGit
var clone = require('lodash.clonedeep')
var normalizePackageData = require('normalize-package-data')
var npm = require('../npm.js')
var andFinishTracker = require('./and-finish-tracker.js')
var flattenTree = require('./flatten-tree.js')
var validateAllPeerDeps = require('./deps.js').validateAllPeerDeps
var packageId = require('../utils/package-id.js')

module.exports = function (idealTree, log, next) {
  validate('OOF', arguments)
  var moduleMap = flattenTree(idealTree)
  var modules = Object.keys(moduleMap).map(function (name) { return moduleMap[name] })

  chain([
    [asyncMap, modules, function (mod, done) {
      chain([
        mod.parent && !mod.isLink && [checkGit, mod.realpath],
        [checkErrors, mod, idealTree]
      ], done)
    }],
    [thenValidateAllPeerDeps, idealTree],
    [thenCheckTop, idealTree],
    [thenCheckDuplicateDeps, idealTree]
  ], andFinishTracker(log, next))
}

function checkErrors (mod, idealTree, next) {
  if (mod.error && (mod.parent || path.resolve(npm.globalDir, '..') !== mod.path)) idealTree.warnings.push(mod.error)
  next()
}

function thenValidateAllPeerDeps (idealTree, next) {
  validate('OF', arguments)
  validateAllPeerDeps(idealTree, function (tree, pkgname, version) {
    var warn = new Error(packageId(tree) + ' requires a peer of ' + pkgname + '@' +
      version + ' but none is installed. You must install peer dependencies yourself.')
    warn.code = 'EPEERINVALID'
    idealTree.warnings.push(warn)
  })
  next()
}

function thenCheckTop (idealTree, next) {
  validate('OF', arguments)
  if (idealTree.package.error) return next()

  // FIXME: when we replace read-package-json with something less magic,
  // this should done elsewhere.
  // As it is, the package has already been normalized and thus some
  // errors are suppressed.
  var pkg = clone(idealTree.package)
  try {
    normalizePackageData(pkg, function (warn) {
      var warnObj = new Error(packageId(idealTree) + ' ' + warn)
      warnObj.code = 'EPACKAGEJSON'
      idealTree.warnings.push(warnObj)
    }, false)
  } catch (er) {
    er.code = 'EPACKAGEJSON'
    idealTree.warnings.push(er)
  }

  var nodeVersion = npm.config.get('node-version')
  if (/-/.test(nodeVersion)) {
    // if this is a prerelease node…
    var warnObj = new Error('You are using a pre-release version of node and things may not work as expected')
    warnObj.code = 'ENODEPRE'
    idealTree.warnings.push(warnObj)
  }

  next()
}

// check for deps duplciated between devdeps and regular deps
function thenCheckDuplicateDeps (idealTree, next) {
  var deps = idealTree.package.dependencies || {}
  var devDeps = idealTree.package.devDependencies || {}

  for (var pkg in devDeps) {
    if (pkg in deps) {
      var warnObj = new Error('The package ' + pkg + ' is included as both a dev and production dependency.')
      warnObj.code = 'EDUPLICATEDEP'
      idealTree.warnings.push(warnObj)
    }
  }

  next()
}
