'use strict'
var path = require('path')
var rimraf = require('rimraf')
var fs = require('graceful-fs')
var mkdirp = require('mkdirp')
var asyncMap = require('slide').asyncMap
var move = require('../../utils/move.js')
var gentlyRm = require('../../utils/gently-rm')
var moduleStagingPath = require('../module-staging-path.js')

module.exports = function (staging, pkg, log, next) {
  log.silly('finalize', pkg.path)

  var extractedTo = moduleStagingPath(staging, pkg)

  var delpath = path.join(path.dirname(pkg.path), '.' + path.basename(pkg.path) + '.DELETE')

  mkdirp(path.resolve(pkg.path, '..'), whenParentExists)

  function whenParentExists (mkdirEr) {
    if (mkdirEr) return next(mkdirEr)
    // We stat first, because we can't rely on ENOTEMPTY from Windows.
    // Windows, by contrast, gives the generic EPERM of a folder already exists.
    fs.lstat(pkg.path, destStatted)
  }

  function destStatted (doesNotExist) {
    if (doesNotExist) {
      move(extractedTo, pkg.path, whenMoved)
    } else {
      moveAway()
    }
  }

  function whenMoved (moveEr) {
    if (!moveEr) return next()
    if (moveEr.code !== 'ENOTEMPTY' && moveEr.code !== 'EEXIST') return next(moveEr)
    moveAway()
  }

  function moveAway () {
    move(pkg.path, delpath, whenOldMovedAway)
  }

  function whenOldMovedAway (moveEr) {
    if (moveEr) return next(moveEr)
    move(extractedTo, pkg.path, whenConflictMoved)
  }

  function whenConflictMoved (moveEr) {
    // if we got an error we'll try to put back the original module back,
    // succeed or fail though we want the original error that caused this
    if (moveEr) return move(delpath, pkg.path, function () { next(moveEr) })
    fs.readdir(path.join(delpath, 'node_modules'), makeTarget)
  }

  function makeTarget (readdirEr, files) {
    if (readdirEr) return cleanup()
    if (!files.length) return cleanup()
    mkdirp(path.join(pkg.path, 'node_modules'), function (mkdirEr) { moveModules(mkdirEr, files) })
  }

  function moveModules (mkdirEr, files) {
    if (mkdirEr) return next(mkdirEr)
    asyncMap(files, function (file, done) {
      var from = path.join(delpath, 'node_modules', file)
      var to = path.join(pkg.path, 'node_modules', file)
      move(from, to, done)
    }, cleanup)
  }

  function cleanup (moveEr) {
    if (moveEr) return next(moveEr)
    rimraf(delpath, afterCleanup)
  }

  function afterCleanup (rimrafEr) {
    if (rimrafEr) log.warn('finalize', rimrafEr)
    next()
  }
}

module.exports.rollback = function (top, staging, pkg, next) {
  gentlyRm(pkg.path, false, top, next)
}
