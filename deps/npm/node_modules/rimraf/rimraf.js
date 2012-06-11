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

var lstat = "lstat"
if (process.platform === "win32") {
  // not reliable on windows prior to 0.7.9
  var v = process.version.replace(/^v/, '').split(/\.|-/).map(Number)
  if (v[0] === 0 && (v[1] < 7 || v[1] == 7 && v[2] < 9)) {
    lstat = "stat"
  }
}
if (!fs[lstat]) lstat = "stat"
var lstatSync = lstat + "Sync"

// for EMFILE handling
var timeout = 0
exports.EMFILE_MAX = 1000
exports.BUSYTRIES_MAX = 3

function rimraf (p, cb) {

  if (!cb) throw new Error("No callback passed to rimraf()")

  var busyTries = 0

  rimraf_(p, function CB (er) {
    if (er) {
      if (er.code === "EBUSY" && busyTries < exports.BUSYTRIES_MAX) {
        busyTries ++
        var time = busyTries * 100
        // try again, with the same exact callback as this one.
        return setTimeout(function () {
          rimraf_(p, CB)
        }, time)
      }

      // this one won't happen if graceful-fs is used.
      if (er.code === "EMFILE" && timeout < exports.EMFILE_MAX) {
        return setTimeout(function () {
          rimraf_(p, CB)
        }, timeout ++)
      }

      // already gone
      if (er.code === "ENOENT") er = null
    }

    timeout = 0
    cb(er)
  })
}

function rimraf_ (p, cb) {
  fs[lstat](p, function (er, s) {
    if (er) {
      // already gone
      if (er.code === "ENOENT") return cb()
      // some other kind of error, permissions, etc.
      return cb(er)
    }

    return rm_(p, s, false, cb)
  })
}


var myGid = function myGid () {
  var g = process.getuid && process.getgid()
  myGid = function myGid () { return g }
  return g
}

var myUid = function myUid () {
  var u = process.getuid && process.getuid()
  myUid = function myUid () { return u }
  return u
}


function writable (s) {
  var mode = s.mode || 0777
    , uid = myUid()
    , gid = myGid()
  return (mode & 0002)
      || (gid === s.gid && (mode & 0020))
      || (uid === s.uid && (mode & 0200))
}

function rm_ (p, s, didWritableCheck, cb) {
  if (!didWritableCheck && !writable(s)) {
    // make file writable
    // user/group/world, doesn't matter at this point
    // since it's about to get nuked.
    return fs.chmod(p, s.mode | 0222, function (er) {
      if (er) return cb(er)
      rm_(p, s, true, cb)
    })
  }

  if (!s.isDirectory()) {
    return fs.unlink(p, cb)
  }

  // directory
  fs.readdir(p, function (er, files) {
    if (er) return cb(er)
    asyncForEach(files.map(function (f) {
      return path.join(p, f)
    }), function (file, cb) {
      rimraf(file, cb)
    }, function (er) {
      if (er) return cb(er)
      fs.rmdir(p, cb)
    })
  })
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

  if (!writable(s)) {
    fs.chmodSync(p, s.mode | 0222)
  }

  if (!s.isDirectory()) return fs.unlinkSync(p)

  fs.readdirSync(p).forEach(function (f) {
    rimrafSync(path.join(p, f))
  })
  fs.rmdirSync(p)
}
