'use strict'
var path = require('path')
var rimraf = require('rimraf')
var fs = require('graceful-fs')
var mkdirp = require('mkdirp')
var asyncMap = require('slide').asyncMap

module.exports = function (top, buildpath, pkg, log, next) {
  log.silly('finalize', pkg.path)

  var delpath = path.join(path.dirname(pkg.path), '.' + path.basename(pkg.path) + '.DELETE')

  mkdirp(path.resolve(pkg.path, '..'), whenParentExists)

  function whenParentExists (mkdirEr) {
    if (mkdirEr) return next(mkdirEr)
    // We stat first, because we can't rely on ENOTEMPTY from Windows.
    // Windows, by contrast, gives the generic EPERM of a folder already exists.
    fs.lstat(pkg.path, destStated)
  }

  function destStated (doesNotExist) {
    if (doesNotExist) {
      fs.rename(buildpath, pkg.path, whenMoved)
    } else {
      moveAway()
    }
  }

  function whenMoved (renameEr) {
    if (!renameEr) return next()
    if (renameEr.code !== 'ENOTEMPTY') return next(renameEr)
    moveAway()
  }

  function moveAway () {
    fs.rename(pkg.path, delpath, whenOldMovedAway)
  }

  function whenOldMovedAway (renameEr) {
    if (renameEr) return next(renameEr)
    fs.rename(buildpath, pkg.path, whenConflictMoved)
  }

  function whenConflictMoved (renameEr) {
    // if we got an error we'll try to put back the original module back,
    // succeed or fail though we want the original error that caused this
    if (renameEr) return fs.rename(delpath, pkg.path, function () { next(renameEr) })
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
      fs.rename(from, to, done)
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

module.exports.rollback = function (buildpath, pkg, next) {
  var top = path.resolve(buildpath, '..')
  rimraf(pkg.path, function () {
    removeEmptyParents(pkg.path)
  })
  function removeEmptyParents (pkgdir) {
    if (path.relative(top, pkgdir)[0] === '.') return next()
    fs.rmdir(pkgdir, function (er) {
      // FIXME: Make sure windows does what we want here
      if (er && er.code !== 'ENOENT') return next()
      removeEmptyParents(path.resolve(pkgdir, '..'))
    })
  }
}
