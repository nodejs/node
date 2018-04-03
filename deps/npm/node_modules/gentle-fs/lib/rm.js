'use strict'

const path = require('path')
const validate = require('aproba')
const fs = require('graceful-fs')
const isInside = require('path-is-inside')
const vacuum = require('fs-vacuum')
const chain = require('slide').chain
const asyncMap = require('slide').asyncMap
const readCmdShim = require('read-cmd-shim')
const iferr = require('iferr')

exports = module.exports = rm

function rm (target, opts, cb) {
  var targetPath = path.normalize(path.resolve(opts.prefix, target))
  if (opts.prefixes.indexOf(targetPath) !== -1) {
    return cb(new Error('May not delete: ' + targetPath))
  }
  var options = {}
  if (opts.force) { options.purge = true }
  if (opts.base) options.base = path.normalize(path.resolve(opts.prefix, opts.base))

  if (!opts.gently) {
    options.purge = true
    return vacuum(targetPath, options, cb)
  }

  var parent = options.base = options.base || path.normalize(opts.prefix)

  // Do all the async work we'll need to do in order to tell if this is a
  // safe operation
  chain([
    [isEverInside, parent, opts.prefixes, opts.log],
    [readLinkOrShim, targetPath],
    [isEverInside, targetPath, opts.prefixes, opts.log],
    [isEverInside, targetPath, [parent], opts.log]
  ], function (er, results) {
    if (er) {
      if (er.code === 'ENOENT') return cb()
      return cb(er)
    }
    var parentInfo = {
      path: parent,
      managed: results[0]
    }
    var targetInfo = {
      path: targetPath,
      symlink: results[1],
      managed: results[2],
      inParent: results[3]
    }

    isSafeToRm(parentInfo, targetInfo, opts.name, opts.log, iferr(cb, thenRemove))

    function thenRemove (toRemove, removeBase) {
      if (!toRemove) return cb()
      if (removeBase) options.base = removeBase
      return vacuum(toRemove, options, cb)
    }
  })
}

exports._isSafeToRm = isSafeToRm
function isSafeToRm (parent, target, pkgName, log, cb) {
  log.silly('gentlyRm', 'parent.path =', parent.path)
  log.silly('gentlyRm', 'parent.managed =',
    parent.managed && parent.managed.target + ' is in ' + parent.managed.path)
  log.silly('gentlyRm', 'target.path = ', target.path)
  log.silly('gentlyRm', 'target.symlink =', target.symlink)
  log.silly('gentlyRm', 'target.managed =',
    target.managed && target.managed.target + ' is in ' + target.managed.path)
  log.silly('gentlyRm', 'target.inParent = ', target.inParent)

  // The parent directory or something it symlinks to must eventually be in
  // a folder that we maintain.
  if (!parent.managed) {
    log.info('gentlyRm', parent.path,
      'is not contained in any directory ' + pkgName + ' is known to control or ' +
      'any place they link to')
    return cb(clobberFail(target.path, 'containing path ' + parent.path +
      " isn't under " + pkgName + "'s control"))
  }

  // The target or something it symlinks to must eventually be in the parent
  // or something the parent symlinks to
  if (target.inParent) {
    var actualTarget = target.inParent.target
    var targetsParent = target.inParent.path
    // if the target.path was what we found in some version of parent, remove
    // using that parent as the base
    if (target.path === actualTarget) {
      return cb(null, target.path, targetsParent)
    } else {
      // If something the target.path links to was what was found, just
      // remove target.path in the location it was found.
      return cb(null, target.path, path.dirname(target.path))
    }
  }

  // If the target is in a managed directory and is in a symlink, but was
  // not in our parent that usually means someone else installed a bin file
  // with the same name as one of our bin files.
  if (target.managed && target.symlink) {
    log.warn('rm', 'not removing', target.path,
      "as it wasn't installed by", parent.path)
    return cb()
  }

  if (target.symlink) {
    return cb(clobberFail(target.path, target.symlink +
      ' symlink target is not controlled by ' + pkgName + ' ' + parent.path))
  } else {
    return cb(clobberFail(target.path, 'is outside ' + parent.path +
      ' and not a link'))
  }
}

function clobberFail (target, msg) {
  validate('SS', arguments)
  var er = new Error('Refusing to delete ' + target + ': ' + msg)
  er.code = 'EEXIST'
  er.path = target
  return er
}

function isENOENT (err) {
  return err && err.code === 'ENOENT'
}

function notENOENT (err) {
  return !isENOENT(err)
}

function skipENOENT (cb) {
  return function (err, value) {
    if (isENOENT(err)) {
      return cb(null, false)
    } else {
      return cb(err, value)
    }
  }
}

function errorsToValues (fn) {
  return function () {
    var args = Array.prototype.slice.call(arguments)
    var cb = args.pop()
    args.push(function (err, value) {
      if (err) {
        return cb(null, err)
      } else {
        return cb(null, value)
      }
    })
    fn.apply(null, args)
  }
}

function isNotError (value) {
  return !(value instanceof Error)
}

exports._isEverInside = isEverInside
// return the first of path, where target (or anything it symlinks to)
// isInside the path (or anything it symlinks to)
function isEverInside (target, paths, log, cb) {
  validate('SAOF', arguments)
  asyncMap(paths, errorsToValues(readAllLinks), iferr(cb, function (resolvedPaths) {
    var errorFree = resolvedPaths.filter(isNotError)
    if (errorFree.length === 0) {
      var badErrors = resolvedPaths.filter(notENOENT)
      if (badErrors.length === 0) {
        return cb(null, false)
      } else {
        return cb(badErrors[0])
      }
    }
    readAllLinks(target, iferr(skipENOENT(cb), function (targets) {
      cb(null, areAnyInsideAny(targets, errorFree, log))
    }))
  }))
}

exports._areAnyInsideAny = areAnyInsideAny
// Return the first path found that any target is inside
function areAnyInsideAny (targets, paths, log) {
  validate('AAO', arguments)
  var toCheck = []
  paths.forEach(function (path) {
    targets.forEach(function (target) {
      toCheck.push([target, path])
    })
  })
  for (var ii = 0; ii < toCheck.length; ++ii) {
    var target = toCheck[ii][0]
    var path = toCheck[ii][1]
    var inside = isInside(target, path)
    if (!inside) log.silly('isEverInside', target, 'is not inside', path)
    if (inside && path) return inside && path && {target: target, path: path}
  }
  return false
}

exports._readAllLinks = readAllLinks
// resolves chains of symlinks of unlimited depth, returning a list of paths
// it's seen in the process when it hits either a symlink cycle or a
// non-symlink
function readAllLinks (path, cb) {
  validate('SF', arguments)
  var seen = {}
  _readAllLinks(path)

  function _readAllLinks (path) {
    if (seen[path]) return cb(null, Object.keys(seen))
    seen[path] = true
    resolveSymlink(path, iferr(cb, _readAllLinks))
  }
}

exports._resolveSymlink = resolveSymlink
var resolvedPaths = {}
function resolveSymlink (symlink, cb) {
  validate('SF', arguments)
  var cached = resolvedPaths[symlink]
  if (cached) return cb(null, cached)

  readLinkOrShim(symlink, iferr(cb, function (symlinkTarget) {
    if (symlinkTarget) {
      resolvedPaths[symlink] = path.resolve(path.dirname(symlink), symlinkTarget)
    } else {
      resolvedPaths[symlink] = symlink
    }
    return cb(null, resolvedPaths[symlink])
  }))
}

exports._readLinkOrShim = readLinkOrShim
function readLinkOrShim (path, cb) {
  validate('SF', arguments)
  fs.lstat(path, iferr(cb, function (stat) {
    if (stat.isSymbolicLink()) {
      fs.readlink(path, cb)
    } else {
      readCmdShim(path, function (er, source) {
        if (!er) return cb(null, source)
        // lstat wouldn't return an error on these, so we don't either.
        if (er.code === 'ENOTASHIM' || er.code === 'EISDIR') {
          return cb(null, null)
        } else {
          return cb(er)
        }
      })
    }
  }))
}
