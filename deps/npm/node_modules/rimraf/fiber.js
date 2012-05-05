// fiber/future port originally written by Marcel Laverdet
// https://gist.github.com/1131093
// I updated it to bring to feature parity with cb version.
// The bugs are probably mine, not Marcel's.
// -- isaacs

var path = require('path')
  , fs = require('fs')
  , Future = require('fibers/future')

// Create future-returning fs functions
var fs2 = {}
for (var ii in fs) {
  fs2[ii] = Future.wrap(fs[ii])
}

// Return a future which just pauses for a certain amount of time

function timer (ms) {
  var future = new Future
  setTimeout(function () {
    future.return()
  }, ms)
  return future
}

function realish (p) {
  return path.resolve(path.dirname(fs2.readlink(p)))
}

// for EMFILE backoff.
var timeout = 0
  , EMFILE_MAX = 1000

function rimraf_ (p, opts) {
  opts = opts || {}
  opts.maxBusyTries = opts.maxBusyTries || 3
  if (opts.gently) opts.gently = path.resolve(opts.gently)
  var busyTries = 0

  // exits by throwing or returning.
  // loops on handled errors.
  while (true) {
    try {
      var stat = fs2.lstat(p).wait()

      // check to make sure that symlinks are ours.
      if (opts.gently) {
        var rp = stat.isSymbolicLink() ? realish(p) : path.resolve(p)
        if (rp.indexOf(opts.gently) !== 0) {
          var er = new Error("Refusing to delete: "+p+" not in "+opts.gently)
          er.errno = require("constants").EEXIST
          er.code = "EEXIST"
          er.path = p
          throw er
        }
      }

      if (!stat.isDirectory()) return fs2.unlink(p).wait()

      var rimrafs = fs2.readdir(p).wait().map(function (file) {
        return rimraf(path.join(p, file), opts)
      })

      Future.wait(rimrafs)
      fs2.rmdir(p).wait()
      timeout = 0
      return

    } catch (er) {
      if (er.message.match(/^EMFILE/) && timeout < EMFILE_MAX) {
        timer(timeout++).wait()
      } else if (er.message.match(/^EBUSY/)
                 && busyTries < opt.maxBusyTries) {
        timer(++busyTries * 100).wait()
      } else if (er.message.match(/^ENOENT/)) {
        // already gone
        return
      } else {
        throw er
      }
    }
  }
}

var rimraf = module.exports = rimraf_.future()
