var fs = require("graceful-fs")
  , assert = require("assert")
  , path = require("path")
  , mkdir = require("mkdirp")
  , chownr = require("chownr")
  , pathIsInside = require("path-is-inside")
  , readJson = require("read-package-json")
  , log = require("npmlog")
  , npm = require("../npm.js")
  , tar = require("../utils/tar.js")
  , deprCheck = require("../utils/depr-check.js")
  , locker = require("../utils/locker.js")
  , lock = locker.lock
  , unlock = locker.unlock
  , getCacheStat = require("./get-stat.js")
  , addNamed = require("./add-named.js")
  , addLocalTarball = require("./add-local-tarball.js")
  , maybeGithub = require("./maybe-github.js")
  , sha = require("sha")

module.exports = addLocal

function addLocal (p, pkgData, cb_) {
  assert(typeof p === "string", "must have path")
  assert(typeof cb === "function", "must have callback")

  pkgData = pkgData || {}

  function cb (er, data) {
    unlock(p, function () {
      if (er) {
        // if it doesn't have a / in it, it might be a
        // remote thing.
        if (p.indexOf("/") === -1 && p.charAt(0) !== "."
           && (process.platform !== "win32" || p.indexOf("\\") === -1)) {
          return addNamed(p, "", null, cb_)
        }
        log.error("addLocal", "Could not install %s", p)
        return cb_(er)
      }
      if (data && !data._fromGithub) data._from = p
      return cb_(er, data)
    })
  }

  lock(p, function (er) {
    if (er) return cb(er)
    // figure out if this is a folder or file.
    fs.stat(p, function (er, s) {
      if (er) {
        // might be username/project
        // in that case, try it as a github url.
        if (p.split("/").length === 2) {
          return maybeGithub(p, er, cb)
        }
        return cb(er)
      }
      if (s.isDirectory()) addLocalDirectory(p, pkgData, null, cb)
      else addLocalTarball(p, pkgData, null, cb)
    })
  })
}

// At this point, if shasum is set, it's something that we've already
// read and checked.  Just stashing it in the data at this point.
function addLocalDirectory (p, pkgData, shasum, cb) {
  assert(pkgData, "must pass package data")
  assert(typeof cb === "function", "must have callback")

  // if it's a folder, then read the package.json,
  // tar it to the proper place, and add the cache tar
  if (pathIsInside(p, npm.cache)) return cb(new Error(
    "Adding a cache directory to the cache will make the world implode."))

  readJson(path.join(p, "package.json"), false, function (er, data) {
    er = needName(er, data)
    er = needVersion(er, data)

    // check that this is what we expected.
    if (!er && pkgData.name && pkgData.name !== data.name) {
      er = new Error( "Invalid Package: expected "
                    + pkgData.name + " but found "
                    + data.name )
    }

    if (!er && pkgData.version && pkgData.version !== data.version) {
      er = new Error( "Invalid Package: expected "
                    + pkgData.name + "@" + pkgData.version
                    + " but found "
                    + data.name + "@" + data.version )
    }

    if (er) return cb(er)
    deprCheck(data)

    // pack to {cache}/name/ver/package.tgz
    var croot = path.resolve(npm.cache, data.name, data.version)
    var tgz = path.resolve(croot, "package.tgz")
    var pj = path.resolve(croot, "package/package.json")
    getCacheStat(function (er, cs) {
      mkdir(path.dirname(pj), function (er, made) {
        if (er) return cb(er)
        var fancy = !pathIsInside(p, npm.tmp)
        tar.pack(tgz, p, data, fancy, function (er) {
          if (er) {
            log.error( "addLocalDirectory", "Could not pack %j to %j"
                     , p, tgz )
            return cb(er)
          }

          if (!cs || isNaN(cs.uid) || isNaN(cs.gid)) next()

          chownr(made || tgz, cs.uid, cs.gid, next)
        })
      })
    })

    function next (er) {
      if (er) return cb(er)
      // if we have the shasum already, just add it
      if (shasum) {
        return addLocalTarball(tgz, data, shasum, cb)
      } else {
        sha.get(tgz, function (er, shasum) {
          if (er) {
            return cb(er)
          }
          data._shasum = shasum
          return addLocalTarball(tgz, data, shasum, cb)
        })
      }
    }
  })
}

function needName(er, data) {
  return er ? er
       : (data && !data.name) ? new Error("No name provided")
       : null
}

function needVersion(er, data) {
  return er ? er
       : (data && !data.version) ? new Error("No version provided")
       : null
}
