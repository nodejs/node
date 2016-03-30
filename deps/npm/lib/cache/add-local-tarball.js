var mkdir = require("mkdirp")
  , assert = require("assert")
  , fs = require("graceful-fs")
  , writeFileAtomic = require("write-file-atomic")
  , path = require("path")
  , sha = require("sha")
  , npm = require("../npm.js")
  , log = require("npmlog")
  , tar = require("../utils/tar.js")
  , pathIsInside = require("path-is-inside")
  , getCacheStat = require("./get-stat.js")
  , cachedPackageRoot = require("./cached-package-root.js")
  , chownr = require("chownr")
  , inflight = require("inflight")
  , once = require("once")
  , writeStreamAtomic = require("fs-write-stream-atomic")
  , randomBytes = require("crypto").pseudoRandomBytes // only need uniqueness

module.exports = addLocalTarball

function addLocalTarball (p, pkgData, shasum, cb) {
  assert(typeof p === "string", "must have path")
  assert(typeof cb === "function", "must have callback")

  if (!pkgData) pkgData = {}

  // If we don't have a shasum yet, compute it.
  if (!shasum) {
    return sha.get(p, function (er, shasum) {
      if (er) return cb(er)
      log.silly("addLocalTarball", "shasum (computed)", shasum)
      addLocalTarball(p, pkgData, shasum, cb)
    })
  }

  if (pathIsInside(p, npm.cache)) {
    if (path.basename(p) !== "package.tgz") {
      return cb(new Error("Not a valid cache tarball name: "+p))
    }
    log.verbose("addLocalTarball", "adding from inside cache", p)
    return addPlacedTarball(p, pkgData, shasum, cb)
  }

  addTmpTarball(p, pkgData, shasum, function (er, data) {
    if (data) {
      data._resolved = p
      data._shasum = data._shasum || shasum
    }
    return cb(er, data)
  })
}

function addPlacedTarball (p, pkgData, shasum, cb) {
  assert(pkgData, "should have package data by now")
  assert(typeof cb === "function", "cb function required")

  getCacheStat(function (er, cs) {
    if (er) return cb(er)
    return addPlacedTarball_(p, pkgData, cs.uid, cs.gid, shasum, cb)
  })
}

function addPlacedTarball_ (p, pkgData, uid, gid, resolvedSum, cb) {
  var folder = path.join(cachedPackageRoot(pkgData), "package")

  // First, make sure we have the shasum, if we don't already.
  if (!resolvedSum) {
    sha.get(p, function (er, shasum) {
      if (er) return cb(er)
      addPlacedTarball_(p, pkgData, uid, gid, shasum, cb)
    })
    return
  }

  mkdir(folder, function (er) {
    if (er) return cb(er)
    var pj = path.join(folder, "package.json")
    var json = JSON.stringify(pkgData, null, 2)
    writeFileAtomic(pj, json, function (er) {
      cb(er, pkgData)
    })
  })
}

function addTmpTarball (tgz, pkgData, shasum, cb) {
  assert(typeof cb === "function", "must have callback function")
  assert(shasum, "must have shasum by now")

  cb = inflight("addTmpTarball:" + tgz, cb)
  if (!cb) return log.verbose("addTmpTarball", tgz, "already in flight; not adding")
  log.verbose("addTmpTarball", tgz, "not in flight; adding")

  // we already have the package info, so just move into place
  if (pkgData && pkgData.name && pkgData.version) {
    log.verbose(
      "addTmpTarball",
      "already have metadata; skipping unpack for",
      pkgData.name + "@" + pkgData.version
    )
    return addTmpTarball_(tgz, pkgData, shasum, cb)
  }

  // This is a tarball we probably downloaded from the internet.  The shasum's
  // already been checked, but we haven't ever had a peek inside, so we unpack
  // it here just to make sure it is what it says it is.
  //
  // NOTE: we might not have any clue what we think it is, for example if the
  // user just did `npm install ./foo.tgz`

  // generate a unique filename
  randomBytes(6, function (er, random) {
    if (er) return cb(er)

    var target = path.join(npm.tmp, "unpack-" + random.toString("hex"))
    getCacheStat(function (er, cs) {
      if (er) return cb(er)

      log.verbose("addTmpTarball", "validating metadata from", tgz)
      tar.unpack(tgz, target, null, null, cs.uid, cs.gid, function (er, data) {
        if (er) return cb(er)

        // check that this is what we expected.
        if (!data.name) {
          return cb(new Error("No name provided"))
        }
        else if (pkgData.name && data.name !== pkgData.name) {
          return cb(new Error("Invalid Package: expected " + pkgData.name +
                              " but found " + data.name))
        }

        if (!data.version) {
          return cb(new Error("No version provided"))
        }
        else if (pkgData.version && data.version !== pkgData.version) {
          return cb(new Error("Invalid Package: expected " +
                              pkgData.name + "@" + pkgData.version +
                              " but found " + data.name + "@" + data.version))
        }

        addTmpTarball_(tgz, data, shasum, cb)
      })
    })
  })
}

function addTmpTarball_ (tgz, data, shasum, cb) {
  assert(typeof cb === "function", "must have callback function")
  cb = once(cb)

  assert(data.name, "should have package name by now")
  assert(data.version, "should have package version by now")

  var root = cachedPackageRoot(data)
  var pkg = path.resolve(root, "package")
  var target = path.resolve(root, "package.tgz")
  getCacheStat(function (er, cs) {
    if (er) return cb(er)
    mkdir(pkg, function (er, created) {

      // chown starting from the first dir created by mkdirp,
      // or the root dir, if none had to be created, so that
      // we know that we get all the children.
      function chown () {
        chownr(created || root, cs.uid, cs.gid, done)
      }

      if (er) return cb(er)
      var read = fs.createReadStream(tgz)
      var write = writeStreamAtomic(target, { mode: npm.modes.file })
      var fin = cs.uid && cs.gid ? chown : done
      read.on("error", cb).pipe(write).on("error", cb).on("close", fin)
    })

  })

  function done() {
    data._shasum = data._shasum || shasum
    cb(null, data)
  }
}
