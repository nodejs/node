'use strict'
var fs = require('graceful-fs')
var path = require('path')
var chain = require('slide').chain
var iferr = require('iferr')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var rmStuff = require('../../unbuild.js').rmStuff
var lifecycle = require('../../utils/lifecycle.js')
var updatePackageJson = require('../update-package-json')

module.exports = function (top, buildpath, pkg, log, next) {
  log.silly('move', pkg.fromPath, pkg.path)
  chain([
    [lifecycle, pkg.package, 'preuninstall', pkg.fromPath, false, true],
    [lifecycle, pkg.package, 'uninstall', pkg.fromPath, false, true],
    [rmStuff, pkg.package, pkg.fromPath],
    [lifecycle, pkg.package, 'postuninstall', pkg.fromPath, false, true],
    [moveModuleOnly, pkg.fromPath, pkg.path],
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

function moveModuleOnly (from, to, done) {
  var from_modules = path.join(from, 'node_modules')
  var temp_modules = from + '.node_modules'

  rimraf(to, iferr(done, makeDestination))

  function makeDestination () {
    mkdirp(path.resolve(to, '..'), iferr(done, moveNodeModules))
  }

  function moveNodeModules () {
    fs.rename(from_modules, temp_modules, function (er) {
      doMove(er ? done : moveNodeModulesBack)
    })
  }

  function doMove (next) {
    fs.rename(from, to, iferr(done, next))
  }

  function moveNodeModulesBack () {
    mkdirp(from, iferr(done, function () {
      fs.rename(temp_modules, from_modules, done)
    }))
  }
}
