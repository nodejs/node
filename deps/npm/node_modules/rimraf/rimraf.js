module.exports = rimraf
rimraf.sync = rimrafSync

var path = require("path")
  , fs

try {
  // optional dependency
  fs = require("graceful-fs")
} catch (er) {
  fs = require("fs")
}

var lstat = process.platform === "win32" ? "stat" : "lstat"
  , lstatSync = lstat + "Sync"

// for EMFILE handling
var timeout = 0
  , EMFILE_MAX = 1000

function rimraf (p, opts, cb) {
  if (typeof opts === "function") cb = opts, opts = {}

  if (!cb) throw new Error("No callback passed to rimraf()")
  if (!opts) opts = {}

  var busyTries = 0
  opts.maxBusyTries = opts.maxBusyTries || 3

  if (opts.gently) opts.gently = path.resolve(opts.gently)

  rimraf_(p, opts, function CB (er) {
    if (er) {
      if (er.code === "EBUSY" && busyTries < opts.maxBusyTries) {
        var time = (opts.maxBusyTries - busyTries) * 100
        busyTries ++
        // try again, with the same exact callback as this one.
        return setTimeout(function () {
          rimraf_(p, opts, CB)
        })
      }

      // this one won't happen if graceful-fs is used.
      if (er.code === "EMFILE" && timeout < EMFILE_MAX) {
        return setTimeout(function () {
          rimraf_(p, opts, CB)
        }, timeout ++)
      }

      // already gone
      if (er.code === "ENOENT") er = null
    }

    timeout = 0
    cb(er)
  })
}

function rimraf_ (p, opts, cb) {
  fs[lstat](p, function (er, s) {
    // if the stat fails, then assume it's already gone.
    if (er) {
      // already gone
      if (er.code === "ENOENT") return cb()
      // some other kind of error, permissions, etc.
      return cb(er)
    }

    // don't delete that don't point actually live in the "gently" path
    if (opts.gently) return clobberTest(p, s, opts, cb)
    return rm_(p, s, opts, cb)
  })
}

function rm_ (p, s, opts, cb) {
  if (!s.isDirectory()) return fs.unlink(p, cb)
  fs.readdir(p, function (er, files) {
    if (er) return cb(er)
    asyncForEach(files.map(function (f) {
      return path.join(p, f)
    }), function (file, cb) {
      rimraf(file, opts, cb)
    }, function (er) {
      if (er) return cb(er)
      fs.rmdir(p, cb)
    })
  })
}

function clobberTest (p, s, opts, cb) {
  var gently = opts.gently
  if (!s.isSymbolicLink()) next(null, path.resolve(p))
  else realish(p, next)

  function next (er, rp) {
    if (er) return rm_(p, s, cb)
    if (rp.indexOf(gently) !== 0) return clobberFail(p, gently, cb)
    else return rm_(p, s, opts, cb)
  }
}

function realish (p, cb) {
  fs.readlink(p, function (er, r) {
    if (er) return cb(er)
    return cb(null, path.resolve(path.dirname(p), r))
  })
}

function clobberFail (p, g, cb) {
  var er = new Error("Refusing to delete: "+p+" not in "+g)
    , constants = require("constants")
  er.errno = constants.EEXIST
  er.code = "EEXIST"
  er.path = p
  return cb(er)
}

function asyncForEach (list, fn, cb) {
  if (!list.length) cb()
  var c = list.length
    , errState = null
  list.forEach(function (item, i, list) {
    fn(item, function (er) {
      if (errState) return
      if (er) return cb(errState = er)
      if (-- c === 0) return cb()
    })
  })
}

// this looks simpler, but it will fail with big directory trees,
// or on slow stupid awful cygwin filesystems
function rimrafSync (p) {
  try {
    var s = fs[lstatSync](p)
  } catch (er) {
    if (er.code === "ENOENT") return
    throw er
  }
  if (!s.isDirectory()) return fs.unlinkSync(p)
  fs.readdirSync(p).forEach(function (f) {
    rimrafSync(path.join(p, f))
  })
  fs.rmdirSync(p)
}
