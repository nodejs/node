'use strict'
var fs = require('graceful-fs')
var path = require('path')
var chain = require('slide').chain
var iferr = require('iferr')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var rmStuff = require('../../unbuild.js').rmStuff
var lifecycle = require('../../utils/lifecycle.js')
var updatePackageJson = require('../update-package-json.js')
var move = require('../../utils/move.js')

/*
  Move a module from one point in the node_modules tree to another.
  Do not disturb either the source or target location's node_modules
  folders.
*/

module.exports = function (staging, pkg, log, next) {
  log.silly('move', pkg.fromPath, pkg.path)
  chain([
    [lifecycle, pkg.package, 'preuninstall', pkg.fromPath, false, true],
    [lifecycle, pkg.package, 'uninstall', pkg.fromPath, false, true],
    [rmStuff, pkg.package, pkg.fromPath],
    [lifecycle, pkg.package, 'postuninstall', pkg.fromPath, false, true],
    [moveModuleOnly, pkg.fromPath, pkg.path, log],
    [lifecycle, pkg.package, 'preinstall', pkg.path, false, true],
    [removeEmptyParents, path.resolve(pkg.fromPath, '..')],
    [updatePackageJson, pkg, pkg.path]
  ], next)
}

function removeEmptyParents (pkgdir, next) {
  fs.rmdir(pkgdir, function (er) {
    // FIXME: Make sure windows does what we want here
    if (er && er.code !== 'ENOENT') return next()
    removeEmptyParents(path.resolve(pkgdir, '..'), next)
  })
}

function moveModuleOnly (from, to, log, done) {
  var fromModules = path.join(from, 'node_modules')
  var tempFromModules = from + '.node_modules'
  var toModules = path.join(to, 'node_modules')
  var tempToModules = to + '.node_modules'

  log.silly('move', 'move existing destination node_modules away', toModules)

  move(toModules, tempToModules, removeDestination(done))

  function removeDestination (next) {
    return function (er) {
      log.silly('move', 'remove existing destination', to)
      if (er) {
        rimraf(to, iferr(next, makeDestination(next)))
      } else {
        rimraf(to, iferr(next, makeDestination(iferr(next, moveToModulesBack(next)))))
      }
    }
  }

  function moveToModulesBack (next) {
    return function () {
      log.silly('move', 'move existing destination node_modules back', toModules)
      move(tempToModules, toModules, iferr(done, next))
    }
  }

  function makeDestination (next) {
    return function () {
      log.silly('move', 'make sure destination parent exists', path.resolve(to, '..'))
      mkdirp(path.resolve(to, '..'), iferr(done, moveNodeModules(next)))
    }
  }

  function moveNodeModules (next) {
    return function () {
      log.silly('move', 'move source node_modules away', fromModules)
      move(fromModules, tempFromModules, iferr(doMove(next), doMove(moveNodeModulesBack(next))))
    }
  }

  function doMove (next) {
    return function () {
      log.silly('move', 'move module dir to final dest', from, to)
      move(from, to, iferr(done, next))
    }
  }

  function moveNodeModulesBack (next) {
    return function () {
      mkdirp(from, iferr(done, function () {
        log.silly('move', 'put source node_modules back', fromModules)
        move(tempFromModules, fromModules, iferr(done, next))
      }))
    }
  }
}
