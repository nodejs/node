var assert = require("assert")
  , path = require("path")
  , mkdir = require("mkdirp")
  , chownr = require("chownr")
  , pathIsInside = require("path-is-inside")
  , readJson = require("read-package-json")
  , log = require("npmlog")
  , npm = require("../npm.js")
  , tar = require("../utils/tar.js")
  , deprCheck = require("../utils/depr-check.js")
  , getCacheStat = require("./get-stat.js")
  , cachedPackageRoot = require("./cached-package-root.js")
  , addLocalTarball = require("./add-local-tarball.js")
  , sha = require("sha")
  , inflight = require("inflight")

module.exports = addLocal

function addLocal (p, pkgData, cb_) {
  assert(typeof p === "object", "must have spec info")
  assert(typeof cb === "function", "must have callback")

  pkgData = pkgData || {}

  function cb (er, data) {
    if (er) {
      log.error("addLocal", "Could not install %s", p.spec)
      return cb_(er)
    }
    if (data && !data._fromGithub) {
      data._from = path.relative(npm.prefix, p.spec) || "."
      var resolved = path.relative(npm.prefix, p.spec)
      if (resolved) data._resolved = "file:"+resolved
    }
    return cb_(er, data)
  }

  if (p.type === "directory") {
    addLocalDirectory(p.spec, pkgData, null, cb)
  }
  else {
    addLocalTarball(p.spec, pkgData, null, cb)
  }
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
    if (er) return cb(er)

    if (!data.name) {
      return cb(new Error("No name provided in package.json"))
    }
    else if (pkgData.name && pkgData.name !== data.name) {
      return cb(new Error(
        "Invalid package: expected " + pkgData.name + " but found " + data.name
      ))
    }

    if (!data.version) {
      return cb(new Error("No version provided in package.json"))
    }
    else if (pkgData.version && pkgData.version !== data.version) {
      return cb(new Error(
        "Invalid package: expected " + pkgData.name + "@" + pkgData.version +
          " but found " + data.name + "@" + data.version
      ))
    }

    deprCheck(data)

    // pack to {cache}/name/ver/package.tgz
    var root = cachedPackageRoot(data)
    var tgz = path.resolve(root, "package.tgz")
    var pj = path.resolve(root, "package/package.json")

    var wrapped = inflight(tgz, next)
    if (!wrapped) return log.verbose("addLocalDirectory", tgz, "already in flight; waiting")
    log.verbose("addLocalDirectory", tgz, "not in flight; packing")

    getCacheStat(function (er, cs) {
      mkdir(path.dirname(pj), function (er, made) {
        if (er) return cb(er)
        var fancy = !pathIsInside(p, npm.tmp)
        tar.pack(tgz, p, data, fancy, function (er) {
          if (er) {
            log.error("addLocalDirectory", "Could not pack", p, "to", tgz)
            return cb(er)
          }

          if (!cs || isNaN(cs.uid) || isNaN(cs.gid)) wrapped()

          chownr(made || tgz, cs.uid, cs.gid, wrapped)
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
