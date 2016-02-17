// only remove the thing if it's a symlink into a specific folder. This is
// a very common use-case of npm's, but not so common elsewhere.

exports = module.exports = gentlyRm

var resolve = require('path').resolve
var dirname = require('path').dirname
var normalize = require('path').normalize
var validate = require('aproba')
var log = require('npmlog')
var lstat = require('graceful-fs').lstat
var readlink = require('graceful-fs').readlink
var isInside = require('path-is-inside')
var vacuum = require('fs-vacuum')
var chain = require('slide').chain
var asyncMap = require('slide').asyncMap
var readCmdShim = require('read-cmd-shim')
var iferr = require('iferr')
var npm = require('../npm.js')

function gentlyRm (target, gently, base, cb) {
  if (!cb) {
    cb = base
    base = undefined
  }

  if (!cb) {
    cb = gently
    gently = false
  }

  log.silly(
    'gentlyRm',
    target,
    'is being', gently ? 'gently removed' : 'purged',
    base ? 'from base ' + base : ''
  )

  // never rm the root, prefix, or bin dirs
  //
  // globals included because of `npm link` -- as far as the package
  // requesting the link is concerned, the linked package is always
  // installed globally
  var prefixes = [
    npm.prefix,
    npm.globalPrefix,
    npm.dir,
    npm.root,
    npm.globalDir,
    npm.bin,
    npm.globalBin
  ]

  var targetPath = normalize(resolve(npm.prefix, target))
  if (prefixes.indexOf(targetPath) !== -1) {
    log.verbose('gentlyRm', targetPath, "is part of npm and can't be removed")
    return cb(new Error('May not delete: ' + targetPath))
  }
  var options = { log: log.silly.bind(log, 'vacuum-fs') }
  if (npm.config.get('force') || !gently) options.purge = true
  if (base) options.base = normalize(resolve(npm.prefix, base))

  if (!gently) {
    log.verbose('gentlyRm', "don't care about contents; nuking", targetPath)
    return vacuum(targetPath, options, cb)
  }

  var parent = options.base = options.base || normalize(npm.prefix)

  // Do all the async work we'll need to do in order to tell if this is a
  // safe operation
  chain([
    [isEverInside, parent, prefixes],
    [readLinkOrShim, targetPath],
    [isEverInside, targetPath, prefixes],
    [isEverInside, targetPath, [parent]]
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

    isSafeToRm(parentInfo, targetInfo, iferr(cb, thenRemove))

    function thenRemove (toRemove, removeBase) {
      if (!toRemove) return cb()
      if (removeBase) options.base = removeBase
      log.verbose('gentlyRm', options.purge ? 'Purging' : 'Vacuuming',
        toRemove, 'up to', options.base)
      return vacuum(toRemove, options, cb)
    }
  })
}

exports._isSafeToRm = isSafeToRm
function isSafeToRm (parent, target, cb) {
  log.silly('gentlyRm', 'parent.path =', parent.path)
  log.silly('gentlyRm', 'parent.managed =',
    parent.managed && parent.managed.target + ' is in ' + parent.managed.path)
  log.silly('gentlyRm', 'target.path = ', target.path)
  log.silly('gentlyRm', 'target.symlink =', target.symlink)
  log.silly('gentlyRm', 'target.managed =',
    target.managed && target.managed.target + ' is in ' + target.managed.path)
  log.silly('gentlyRm', 'target.inParent = ', target.inParent)

  // The parent directory or something it symlinks to must eventually be in
  // a folder that npm maintains.
  if (!parent.managed) {
    log.verbose('gentlyRm', parent.path,
      'is not contained in any diretory npm is known to control or ' +
      'any place they link to')
    return cb(clobberFail(target.path, 'containing path ' + parent.path +
      " isn't under npm's control"))
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
      return cb(null, target.path, dirname(target.path))
    }
  }

  // If the target is in a managed directory and is in a symlink, but was
  // not in our parent that usually means someone else installed a bin file
  // with the same name as one of our bin files.
  if (target.managed && target.symlink) {
    log.warn('gentlyRm', 'not removing', target.path,
      "as it wasn't installed by", parent.path)
    return cb()
  }

  if (target.symlink) {
    return cb(clobberFail(target.path, target.symlink +
      ' symlink target is not controlled by npm ' + parent.path))
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
function isEverInside (target, paths, cb) {
  validate('SAF', arguments)
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
      cb(null, areAnyInsideAny(targets, errorFree))
    }))
  }))
}

exports._areAnyInsideAny = areAnyInsideAny
// Return the first path found that any target is inside
function areAnyInsideAny (targets, paths) {
  validate('AA', arguments)
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
      resolvedPaths[symlink] = resolve(dirname(symlink), symlinkTarget)
    } else {
      resolvedPaths[symlink] = symlink
    }
    return cb(null, resolvedPaths[symlink])
  }))
}

exports._readLinkOrShim = readLinkOrShim
function readLinkOrShim (path, cb) {
  validate('SF', arguments)
  lstat(path, iferr(cb, function (stat) {
    if (stat.isSymbolicLink()) {
      readlink(path, cb)
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
