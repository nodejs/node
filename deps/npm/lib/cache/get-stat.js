var mkdir = require("mkdirp")
  , fs = require("graceful-fs")
  , log = require("npmlog")
  , chownr = require("chownr")
  , npm = require("../npm.js")
  , inflight = require("inflight")

// to maintain the cache dir's permissions consistently.
var cacheStat = null
module.exports = function getCacheStat (cb) {
  if (cacheStat) return cb(null, cacheStat)

  fs.stat(npm.cache, function (er, st) {
    if (er) return makeCacheDir(cb)
    if (!st.isDirectory()) {
      log.error("getCacheStat", "invalid cache dir %j", npm.cache)
      return cb(er)
    }
    return cb(null, cacheStat = st)
  })
}

function makeCacheDir (cb) {
  cb = inflight("makeCacheDir", cb)
  if (!cb) {
    return log.verbose(
      "getCacheStat",
      "cache creation already in flight; waiting"
    )
  }
  log.verbose("getCacheStat", "cache creation not in flight; initializing")

  if (!process.getuid) return mkdir(npm.cache, function (er) {
    log.verbose("makeCacheDir", "UID & GID are irrelevant on", process.platform)
    cacheStat = { uid : 0, gid : 0 }
    return cb(er, cacheStat)
  })

  var uid = +process.getuid()
    , gid = +process.getgid()

  if (uid === 0) {
    if (process.env.SUDO_UID) uid = +process.env.SUDO_UID
    if (process.env.SUDO_GID) gid = +process.env.SUDO_GID
  }

  if (uid !== 0 || !process.env.HOME) {
    cacheStat = { uid : uid, gid : gid }
    return mkdir(npm.cache, afterMkdir)
  }

  fs.stat(process.env.HOME, function (er, st) {
    if (er) {
      log.error("makeCacheDir", "homeless?")
      return cb(er)
    }
    cacheStat = st
    log.silly("makeCacheDir", "cache dir uid, gid", [st.uid, st.gid])
    return mkdir(npm.cache, afterMkdir)
  })

  function afterMkdir (er, made) {
    if (er || !cacheStat || isNaN(cacheStat.uid) || isNaN(cacheStat.gid)) {
      return cb(er, cacheStat)
    }

    if (!made) return cb(er, cacheStat)

    // ensure that the ownership is correct.
    chownr(made, cacheStat.uid, cacheStat.gid, function (er) {
      return cb(er, cacheStat)
    })
  }
}
