var mkdir = require("mkdirp")
  , assert = require("assert")
  , fs = require("graceful-fs")
  , readJson = require("read-package-json")
  , log = require("npmlog")
  , path = require("path")
  , sha = require("sha")
  , npm = require("../npm.js")
  , tar = require("../utils/tar.js")
  , pathIsInside = require("path-is-inside")
  , locker = require("../utils/locker.js")
  , lock = locker.lock
  , unlock = locker.unlock
  , getCacheStat = require("./get-stat.js")
  , chownr = require("chownr")
  , inflight = require("inflight")
  , once = require("once")

module.exports = addLocalTarball

function addLocalTarball (p, pkgData, shasum, cb_) {
  assert(typeof p === "string", "must have path")
  assert(typeof cb_ === "function", "must have callback")

  if (!pkgData) pkgData = {}
  var name = pkgData.name || ""

  // If we don't have a shasum yet, then get the shasum now.
  if (!shasum) {
    return sha.get(p, function (er, shasum) {
      if (er) return cb_(er)
      addLocalTarball(p, pkgData, shasum, cb_)
    })
  }

  // if it's a tar, and not in place,
  // then unzip to .tmp, add the tmp folder, and clean up tmp
  if (pathIsInside(p, npm.tmp))
    return addTmpTarball(p, pkgData, shasum, cb_)

  if (pathIsInside(p, npm.cache)) {
    if (path.basename(p) !== "package.tgz") return cb_(new Error(
      "Not a valid cache tarball name: "+p))
    return addPlacedTarball(p, pkgData, shasum, cb_)
  }

  function cb (er, data) {
    if (data) {
      data._resolved = p
      data._shasum = data._shasum || shasum
    }
    return cb_(er, data)
  }

  // just copy it over and then add the temp tarball file.
  var tmp = path.join(npm.tmp, name + Date.now()
                             + "-" + Math.random(), "tmp.tgz")
  mkdir(path.dirname(tmp), function (er) {
    if (er) return cb(er)
    var from = fs.createReadStream(p)
      , to = fs.createWriteStream(tmp)
      , errState = null
    function errHandler (er) {
      if (errState) return
      return cb(errState = er)
    }
    from.on("error", errHandler)
    to.on("error", errHandler)
    to.on("close", function () {
      if (errState) return
      log.verbose("chmod", tmp, npm.modes.file.toString(8))
      fs.chmod(tmp, npm.modes.file, function (er) {
        if (er) return cb(er)
        addTmpTarball(tmp, pkgData, shasum, cb)
      })
    })
    from.pipe(to)
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
  // now we know it's in place already as .cache/name/ver/package.tgz
  var name = pkgData.name
    , version = pkgData.version
    , folder = path.join(npm.cache, name, version, "package")

  // First, make sure we have the shasum, if we don't already.
  if (!resolvedSum) {
    sha.get(p, function (er, shasum) {
      if (er) return cb(er)
      addPlacedTarball_(p, pkgData, uid, gid, shasum, cb)
    })
    return
  }

  lock(folder, function (er) {
    if (er) return cb(er)

    // async try/finally
    var originalCb = cb
    cb = function (er, data) {
      unlock(folder, function (er2) {
        return originalCb(er || er2, data)
      })
    }

    mkdir(folder, function (er) {
      if (er) return cb(er)
      var pj = path.join(folder, "package.json")
      var json = JSON.stringify(pkgData, null, 2)
      fs.writeFile(pj, json, "utf8", function (er) {
        cb(er, pkgData)
      })
    })
  })
}

function addTmpTarball (tgz, pkgData, shasum, cb) {
  assert(typeof cb === "function", "must have callback function")
  assert(shasum, "should have shasum by now")

  cb = inflight("addTmpTarball:" + tgz, cb)
  if (!cb) return

  // we already have the package info, so just move into place
  if (pkgData && pkgData.name && pkgData.version) {
    return addTmpTarball_(tgz, pkgData, shasum, cb)
  }

  // This is a tarball we probably downloaded from the internet.
  // The shasum's already been checked, but we haven't ever had
  // a peek inside, so we unpack it here just to make sure it is
  // what it says it is.
  // Note: we might not have any clue what we think it is, for
  // example if the user just did `npm install ./foo.tgz`

  var target = tgz + "-unpack"
  getCacheStat(function (er, cs) {
    tar.unpack(tgz, target, null, null, cs.uid, cs.gid, next)
  })

  function next (er) {
    if (er) return cb(er)
    var pj = path.join(target, "package.json")
    readJson(pj, function (er, data) {
      // XXX dry with similar stanza in add-local.js
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

      addTmpTarball_(tgz, data, shasum, cb)
    })
  }
}

function addTmpTarball_ (tgz, data, shasum, cb) {
  assert(typeof cb === "function", "must have callback function")
  cb = once(cb)

  var name = data.name
  var version = data.version
  assert(name, "should have package name by now")
  assert(version, "should have package version by now")

  var root = path.resolve(npm.cache, name, version)
  var pkg = path.resolve(root, "package")
  var target = path.resolve(root, "package.tgz")
  getCacheStat(function (er, cs) {
    if (er) return cb(er)
    mkdir(pkg, function (er) {
      if (er) return cb(er)
      var read = fs.createReadStream(tgz)
      var write = fs.createWriteStream(target)
      var fin = cs.uid && cs.gid ? chown : done
      read.on("error", cb).pipe(write).on("error", cb).on("close", fin)
    })

    function chown () {
      chownr(root, cs.uid, cs.gid, done)
    }
  })

  function done() {
    data._shasum = data._shasum || shasum
    cb(null, data)
  }
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
