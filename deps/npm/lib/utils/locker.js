var crypto = require("crypto")
var resolve = require("path").resolve

var lockfile = require("lockfile")
var log = require("npmlog")
var mkdirp = require("mkdirp")

var npm = require("../npm.js")
var correctMkdir = require('../utils/correct-mkdir.js')

var installLocks = {}

function lockFileName (base, name) {
  var c = name.replace(/[^a-zA-Z0-9]+/g, "-").replace(/^-+|-+$/g, "")
    , p = resolve(base, name)
    , h = crypto.createHash("sha1").update(p).digest("hex")
    , l = resolve(npm.cache, "_locks")

  return resolve(l, c.substr(0, 24)+"-"+h.substr(0, 16)+".lock")
}

function lock (base, name, cb) {
  var lockDir = resolve(npm.cache, "_locks")
  correctMkdir(lockDir, function (er) {
    if (er) return cb(er)

    var opts = { stale:   npm.config.get("cache-lock-stale")
               , retries: npm.config.get("cache-lock-retries")
               , wait:    npm.config.get("cache-lock-wait") }
    var lf = lockFileName(base, name)
    lockfile.lock(lf, opts, function (er) {
      if (er) log.warn("locking", lf, "failed", er)

      if (!er) {
        log.verbose("lock", "using", lf, "for", resolve(base, name))
        installLocks[lf] = true
      }

      cb(er)
    })
  })
}

function unlock (base, name, cb) {
  var lf = lockFileName(base, name)
    , locked = installLocks[lf]
  if (locked === false) {
    return process.nextTick(cb)
  }
  else if (locked === true) {
    lockfile.unlock(lf, function (er) {
      if (er) {
        log.warn("unlocking", lf, "failed", er)
      }
      else {
        installLocks[lf] = false
        log.verbose("unlock", "done using", lf, "for", resolve(base, name))
      }

      cb(er)
    })
  }
  else {
    throw new Error(
      "Attempt to unlock " + resolve(base, name) + ", which hasn't been locked"
    )
  }
}

module.exports = {
  lock   : lock,
  unlock : unlock
}
