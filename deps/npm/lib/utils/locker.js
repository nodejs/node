var crypto = require("crypto")
var path = require("path")

var npm = require("../npm.js")
var lockFile = require("lockfile")
var log = require("npmlog")
var getCacheStat = require("../cache/get-stat.js")

function lockFileName (u) {
  var c = u.replace(/[^a-zA-Z0-9]+/g, "-").replace(/^-+|-+$/g, "")
    , h = crypto.createHash("sha1").update(u).digest("hex")
  h = h.substr(0, 8)
  c = c.substr(-32)
  log.silly("lockFile", h + "-" + c, u)
  return path.resolve(npm.config.get("cache"), h + "-" + c + ".lock")
}

var myLocks = {}
function lock (u, cb) {
  // the cache dir needs to exist already for this.
  getCacheStat(function (er, cs) {
    if (er) return cb(er)
    var opts = { stale: npm.config.get("cache-lock-stale")
               , retries: npm.config.get("cache-lock-retries")
               , wait: npm.config.get("cache-lock-wait") }
    var lf = lockFileName(u)
    log.verbose("lock", u, lf)
    lockFile.lock(lf, opts, function(er) {
      if (!er) myLocks[lf] = true
      cb(er)
    })
  })
}

function unlock (u, cb) {
  var lf = lockFileName(u)
    , locked = myLocks[lf]
  if (locked === false) {
    return process.nextTick(cb)
  } else if (locked === true) {
    myLocks[lf] = false
    lockFile.unlock(lockFileName(u), cb)
  } else {
    throw new Error("Attempt to unlock " + u + ", which hasn't been locked")
  }
}

module.exports = {
  lock: lock,
  unlock: unlock,
  _lockFileName: lockFileName
}
