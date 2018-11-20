// only remove the thing if it's a symlink into a specific folder.
// This is a very common use-case of npm's, but not so common elsewhere.

module.exports = gentlyRm

var npm = require('../npm.js')
var log = require('npmlog')
var resolve = require('path').resolve
var dirname = require('path').dirname
var lstat = require('graceful-fs').lstat
var readlink = require('graceful-fs').readlink
var isInside = require('path-is-inside')
var vacuum = require('fs-vacuum')
var some = require('async-some')
var asyncMap = require('slide').asyncMap
var normalize = require('path').normalize

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
  // globals included because of `npm link` -- as far as the package requesting
  // the link is concerned, the linked package is always installed globally
  var prefixes = [
    npm.prefix,
    npm.globalPrefix,
    npm.dir,
    npm.root,
    npm.globalDir,
    npm.bin,
    npm.globalBin
  ]

  var resolved = normalize(resolve(npm.prefix, target))
  if (prefixes.indexOf(resolved) !== -1) {
    log.verbose('gentlyRm', resolved, "is part of npm and can't be removed")
    return cb(new Error('May not delete: ' + resolved))
  }

  var options = { log: log.silly.bind(log, 'vacuum-fs') }
  if (npm.config.get('force') || !gently) options.purge = true
  if (base) options.base = normalize(resolve(npm.prefix, base))

  if (!gently) {
    log.verbose('gentlyRm', "don't care about contents; nuking", resolved)
    return vacuum(resolved, options, cb)
  }

  var parent = options.base = normalize(base ? resolve(npm.prefix, base) : npm.prefix)

  // is the parent directory managed by npm?
  log.silly('gentlyRm', 'verifying', parent, 'is an npm working directory')
  some(prefixes, isManaged(parent), function (er, matched) {
    if (er) return cb(er)

    if (!matched) {
      log.error('gentlyRm', 'containing path', parent, "isn't under npm's control")
      return clobberFail(resolved, parent, cb)
    }
    log.silly('gentlyRm', 'containing path', parent, "is under npm's control, in", matched)

    // is the target directly contained within the (now known to be
    // managed) parent?
    if (isInside(resolved, parent)) {
      log.silly('gentlyRm', 'deletion target', resolved, 'is under', parent)
      log.verbose('gentlyRm', 'vacuuming from', resolved, 'up to', parent)
      return vacuum(resolved, options, cb)
    }
    log.silly('gentlyRm', resolved, 'is not under', parent)

    // the target isn't directly within the parent, but is it itself managed?
    log.silly('gentlyRm', 'verifying', resolved, 'is an npm working directory')
    some(prefixes, isManaged(resolved), function (er, matched) {
      if (er) return cb(er)

      if (matched) {
        log.silly('gentlyRm', resolved, "is under npm's control, in", matched)
        options.base = matched
        log.verbose('gentlyRm', 'removing', resolved, 'with base', options.base)
        return vacuum(resolved, options, cb)
      }
      log.verbose('gentlyRm', resolved, "is not under npm's control")

      // the target isn't managed directly, but maybe it's a link...
      log.silly('gentlyRm', 'checking to see if', resolved, 'is a link')
      lstat(resolved, function (er, stat) {
        if (er) {
          // race conditions are common when unbuilding
          if (er.code === 'ENOENT') return cb(null)
          return cb(er)
        }

        if (!stat.isSymbolicLink()) {
          log.error('gentlyRm', resolved, 'is outside', parent, 'and not a link')
          return clobberFail(resolved, parent, cb)
        }

        // ...and maybe the link source, when read...
        log.silly('gentlyRm', resolved, 'is a link')
        readlink(resolved, function (er, link) {
          if (er) {
            // race conditions are common when unbuilding
            if (er.code === 'ENOENT') return cb(null)
            return cb(er)
          }

          // ...is inside the managed parent
          var source = resolve(dirname(resolved), link)
          if (isInside(source, parent)) {
            log.silly('gentlyRm', source, 'symlink target', resolved, 'is inside', parent)
            log.verbose('gentlyRm', 'vacuuming', resolved)
            return vacuum(resolved, options, cb)
          }

          log.error('gentlyRm', source, 'symlink target', resolved, 'is not controlled by npm', parent)
          return clobberFail(target, parent, cb)
        })
      })
    })
  })
}

var resolvedPaths = {}
function isManaged (target) {
  return function predicate (path, cb) {
    if (!path) {
      log.verbose('isManaged', 'no path passed for target', target)
      return cb(null, false)
    }

    asyncMap([path, target], resolveSymlink, function (er, results) {
      if (er) {
        if (er.code === 'ENOENT') return cb(null, false)

        return cb(er)
      }

      var path = results[0]
      var target = results[1]
      var inside = isInside(target, path)
      if (!inside) log.silly('isManaged', target, 'is not inside', path)

      return cb(null, inside && path)
    })
  }

  function resolveSymlink (toResolve, cb) {
    var resolved = resolve(npm.prefix, toResolve)

    // if the path has already been memoized, return immediately
    var cached = resolvedPaths[resolved]
    if (cached) return cb(null, cached)

    // otherwise, check the path
    lstat(resolved, function (er, stat) {
      if (er) return cb(er)

      // if it's not a link, cache & return the path itself
      if (!stat.isSymbolicLink()) {
        resolvedPaths[resolved] = resolved
        return cb(null, resolved)
      }

      // otherwise, cache & return the link's source
      readlink(resolved, function (er, source) {
        if (er) return cb(er)

        resolved = resolve(resolved, source)
        resolvedPaths[resolved] = resolved
        cb(null, resolved)
      })
    })
  }
}

function clobberFail (target, root, cb) {
  var er = new Error('Refusing to delete: ' + target + ' not in ' + root)
  er.code = 'EEXIST'
  er.path = target
  return cb(er)
}
