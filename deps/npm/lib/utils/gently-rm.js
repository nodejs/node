// only remove the thing if it's a symlink into a specific folder.
// This is a very common use-case of npm's, but not so common elsewhere.

module.exports = gentlyRm

var npm = require("../npm.js")
  , log = require("npmlog")
  , resolve = require("path").resolve
  , dirname = require("path").dirname
  , lstat = require("graceful-fs").lstat
  , readlink = require("graceful-fs").readlink
  , isInside = require("path-is-inside")
  , vacuum = require("fs-vacuum")
  , rimraf = require("rimraf")
  , some = require("async-some")

function gentlyRm (path, gently, cb) {
  if (!cb) {
    cb = gently
    gently = null
  }

  // never rm the root, prefix, or bin dirs.
  // just a safety precaution.
  var prefixes = [
    npm.dir,       npm.root,       npm.bin,       npm.prefix,
    npm.globalDir, npm.globalRoot, npm.globalBin, npm.globalPrefix
  ]

  var resolved = resolve(path)
  if (prefixes.indexOf(resolved) !== -1) {
    log.verbose("gentlyRm", resolved, "is part of npm and can't be removed")
    return cb(new Error("May not delete: "+resolved))
  }

  var options = {log : log.silly.bind(log, "gentlyRm")}
  if (npm.config.get("force") || !gently) options.purge = true

  if (!gently) {
    log.verbose("gentlyRm", "vacuuming", resolved)
    return vacuum(resolved, options, cb)
  }

  var parent = resolve(gently)
  log.verbose("gentlyRm", "verifying that", parent, "is managed by npm")
  some(prefixes, isManaged(parent), function (er, matched) {
    if (er) return cb(er)

    if (!matched) {
      log.verbose("gentlyRm", parent, "is not managed by npm")
      return clobberFail(resolved, parent, cb)
    }

    log.silly("gentlyRm", parent, "is managed by npm")

    if (isInside(resolved, parent)) {
      log.silly("gentlyRm", resolved, "is under", parent)
      log.verbose("gentlyRm", "vacuuming", resolved, "up to", parent)
      options.base = parent
      return vacuum(resolved, options, cb)
    }

    log.silly("gentlyRm", resolved, "is not under", parent)
    log.silly("gentlyRm", "checking to see if", resolved, "is a link")
    lstat(resolved, function (er, stat) {
      if (er) {
        if (er.code === "ENOENT") return cb(null)
        return cb(er)
      }

      if (!stat.isSymbolicLink()) {
        log.verbose("gentlyRm", resolved, "is outside", parent, "and not a link")
        return clobberFail(resolved, parent, cb)
      }

      log.silly("gentlyRm", resolved, "is a link")
      readlink(resolved, function (er, link) {
        if (er) {
          if (er.code === "ENOENT") return cb(null)
          return cb(er)
        }

        var source = resolve(dirname(resolved), link)
        if (isInside(source, parent)) {
          log.silly("gentlyRm", source, "inside", parent)
          log.verbose("gentlyRm", "vacuuming", resolved)
          return vacuum(resolved, options, cb)
        }

        log.silly("gentlyRm", "checking to see if", source, "is managed by npm")
        some(prefixes, isManaged(source), function (er, matched) {
          if (er) return cb(er)

          if (matched) {
            log.silly("gentlyRm", source, "is under", matched)
            log.verbose("gentlyRm", "removing", resolved)
            rimraf(resolved, cb)
          }

          log.verbose("gentlyRm", source, "is not managed by npm")
          return clobberFail(path, parent, cb)
        })
      })
    })
  })
}

var resolvedPaths = {}
function isManaged (target) {
  return predicate

  function predicate (path, cb) {
    if (!path) {
      log.verbose("isManaged", "no path")
      return cb(null, false)
    }

    path = resolve(path)

    // if the path has already been memoized, return immediately
    var resolved = resolvedPaths[path]
    if (resolved) {
      var inside = isInside(target, resolved)
      log.silly("isManaged", target, inside ? "is" : "is not", "inside", resolved)

      return cb(null, inside && path)
    }

    // otherwise, check the path
    lstat(path, function (er, stat) {
      if (er) {
        if (er.code === "ENOENT") return cb(null, false)

        return cb(er)
      }

      // if it's not a link, cache & test the path itself
      if (!stat.isSymbolicLink()) return cacheAndTest(path, path, target, cb)

      // otherwise, cache & test the link's source
      readlink(path, function (er, source) {
        if (er) {
          if (er.code === "ENOENT") return cb(null, false)

          return cb(er)
        }

        cacheAndTest(resolve(path, source), path, target, cb)
      })
    })
  }

  function cacheAndTest (resolved, source, target, cb) {
    resolvedPaths[source] = resolved
    var inside = isInside(target, resolved)
    log.silly("cacheAndTest", target, inside ? "is" : "is not", "inside", resolved)
    cb(null, inside && source)
  }
}

function clobberFail (p, g, cb) {
  var er = new Error("Refusing to delete: "+p+" not in "+g)
  er.code = "EEXIST"
  er.path = p
  return cb(er)
}
