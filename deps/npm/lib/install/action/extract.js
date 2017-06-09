'use strict'
var path = require('path')
var iferr = require('iferr')
var asyncMap = require('slide').asyncMap
var fs = require('graceful-fs')
var mkdirp = require('mkdirp')
var move = require('../../utils/move.js')
var gentlyRm = require('../../utils/gently-rm.js')
var updatePackageJson = require('../update-package-json')
var npm = require('../../npm.js')
var moduleName = require('../../utils/module-name.js')
var packageId = require('../../utils/package-id.js')
var cache = require('../../cache.js')
var moduleStagingPath = require('../module-staging-path.js')
var readPackageJson = require('read-package-json')

module.exports = function (staging, pkg, log, next) {
  log.silly('extract', packageId(pkg))
  var up = npm.config.get('unsafe-perm')
  var user = up ? null : npm.config.get('user')
  var group = up ? null : npm.config.get('group')
  var extractTo = moduleStagingPath(staging, pkg)
  cache.unpack(pkg.package.name, pkg.package.version, extractTo, null, null, user, group,
      andUpdatePackageJson(pkg, staging, extractTo,
      andStageBundledChildren(pkg, staging, extractTo, log,
      andRemoveExtraneousBundles(extractTo, next))))
}

function andUpdatePackageJson (pkg, staging, extractTo, next) {
  return iferr(next, function () {
    readPackageJson(path.join(extractTo, 'package.json'), false, function (err, metadata) {
      if (!err) {
        // Copy _ keys (internal to npm) and any missing keys from the possibly incomplete
        // registry metadata over to the full package metadata read off of disk.
        Object.keys(pkg.package).forEach(function (key) {
          if (key[0] === '_' || !(key in metadata)) metadata[key] = pkg.package[key]
        })
        metadata.name = pkg.package.name // things go wrong if these don't match
        pkg.package = metadata
      }
      updatePackageJson(pkg, extractTo, next)
    })
  })
}

function andStageBundledChildren (pkg, staging, extractTo, log, next) {
  return iferr(next, function () {
    if (!pkg.package.bundleDependencies) return next()

    asyncMap(pkg.children, andStageBundledModule(pkg, staging, extractTo), next)
  })
}

function andRemoveExtraneousBundles (extractTo, next) {
  return iferr(next, function () {
    gentlyRm(path.join(extractTo, 'node_modules'), next)
  })
}

function andStageBundledModule (bundler, staging, parentPath) {
  return function (child, next) {
    if (child.error) return next(child.error)
    stageBundledModule(bundler, child, staging, parentPath, next)
  }
}

function getTree (pkg) {
  while (pkg.parent) pkg = pkg.parent
  return pkg
}

function warn (pkg, code, msg) {
  var tree = getTree(pkg)
  var err = new Error(msg)
  err.code = code
  tree.warnings.push(err)
}

function stageBundledModule (bundler, child, staging, parentPath, next) {
  var stageFrom = path.join(parentPath, 'node_modules', child.package.name)
  var stageTo = moduleStagingPath(staging, child)

  return asyncMap(child.children, andStageBundledModule(bundler, staging, stageFrom), iferr(next, finishModule))

  function finishModule () {
    // If we were the one's who bundled this module…
    if (child.fromBundle === bundler) {
      return moveModule()
    } else {
      return checkForReplacement()
    }
  }

  function moveModule () {
    return mkdirp(path.dirname(stageTo), iferr(next, function () {
      return move(stageFrom, stageTo, iferr(next, updateMovedPackageJson))
    }))
  }

  function checkForReplacement () {
    return fs.stat(stageFrom, function (notExists, exists) {
      if (exists) {
        warn(bundler, 'EBUNDLEOVERRIDE', 'In ' + packageId(bundler) +
          ' replacing bundled version of ' + moduleName(child) +
          ' with ' + packageId(child))
        return gentlyRm(stageFrom, next)
      } else {
        return next()
      }
    })
  }

  function updateMovedPackageJson () {
    updatePackageJson(child, stageTo, next)
  }
}
