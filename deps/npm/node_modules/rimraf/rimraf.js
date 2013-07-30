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

// for EMFILE handling
var timeout = 0
exports.EMFILE_MAX = 1000
exports.BUSYTRIES_MAX = 3

var isWindows = (process.platform === "win32")

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

// Two possible strategies.
// 1. Assume it's a file.  unlink it, then do the dir stuff on EPERM or EISDIR
// 2. Assume it's a directory.  readdir, then do the file stuff on ENOTDIR
//
// Both result in an extra syscall when you guess wrong.  However, there
// are likely far more normal files in the world than directories.  This
// is based on the assumption that a the average number of files per
// directory is >= 1.
//
// If anyone ever complains about this, then I guess the strategy could
// be made configurable somehow.  But until then, YAGNI.
function rimraf_ (p, cb) {
  fs.unlink(p, function (er) {
    if (er) {
      if (er.code === "ENOENT")
        return cb()
      if (er.code === "EPERM")
        return (isWindows) ? fixWinEPERM(p, er, cb) : rmdir(p, er, cb)
      if (er.code === "EISDIR")
        return rmdir(p, er, cb)
    }
    return cb(er)
  })
}

function fixWinEPERM (p, er, cb) {
  fs.chmod(p, 666, function (er2) {
    if (er2)
      cb(er2.code === "ENOENT" ? null : er)
    else
      fs.stat(p, function(er3, stats) {
        if (er3)
          cb(er3.code === "ENOENT" ? null : er)
        else if (stats.isDirectory())
          rmdir(p, er, cb)
        else
          fs.unlink(p, cb)
      })
  })
}

function fixWinEPERMSync (p, er, cb) {
  try {
    fs.chmodSync(p, 666)
  } catch (er2) {
    if (er2.code !== "ENOENT")
      throw er
  }

  try {
    var stats = fs.statSync(p)
  } catch (er3) {
    if (er3 !== "ENOENT")
      throw er
  }

  if (stats.isDirectory())
    rmdirSync(p, er)
  else
    fs.unlinkSync(p)
}

function rmdir (p, originalEr, cb) {
  // try to rmdir first, and only readdir on ENOTEMPTY or EEXIST (SunOS)
  // if we guessed wrong, and it's not a directory, then
  // raise the original error.
  fs.rmdir(p, function (er) {
    if (er && (er.code === "ENOTEMPTY" || er.code === "EEXIST"))
      rmkids(p, cb)
    else if (er && er.code === "ENOTDIR")
      cb(originalEr)
    else
      cb(er)
  })
}

function rmkids(p, cb) {
  fs.readdir(p, function (er, files) {
    if (er)
      return cb(er)
    var n = files.length
    if (n === 0)
      return fs.rmdir(p, cb)
    var errState
    files.forEach(function (f) {
      rimraf(path.join(p, f), function (er) {
        if (errState)
          return
        if (er)
          return cb(errState = er)
        if (--n === 0)
          fs.rmdir(p, cb)
      })
    })
  })
}

// this looks simpler, and is strictly *faster*, but will
// tie up the JavaScript thread and fail on excessively
// deep directory trees.
function rimrafSync (p) {
  try {
    fs.unlinkSync(p)
  } catch (er) {
    if (er.code === "ENOENT")
      return
    if (er.code === "EPERM")
      return isWindows ? fixWinEPERMSync(p, er) : rmdirSync(p, er)
    if (er.code !== "EISDIR")
      throw er
    rmdirSync(p, er)
  }
}

function rmdirSync (p, originalEr) {
  try {
    fs.rmdirSync(p)
  } catch (er) {
    if (er.code === "ENOENT")
      return
    if (er.code === "ENOTDIR")
      throw originalEr
    if (er.code === "ENOTEMPTY" || er.code === "EEXIST")
      rmkidsSync(p)
  }
}

function rmkidsSync (p) {
  fs.readdirSync(p).forEach(function (f) {
    rimrafSync(path.join(p, f))
  })
  fs.rmdirSync(p)
}
