var fs = require('fs')

var wx = 'wx'
if (process.version.match(/^v0.[456]/)) {
  var c = require('constants')
  wx = c.O_TRUNC | c.O_CREAT | c.O_WRONLY | c.O_EXCL
}

var locks = {}

function hasOwnProperty (obj, prop) {
  return Object.prototype.hasOwnProperty.call(obj, prop)
}

process.on('exit', function () {
  // cleanup
  Object.keys(locks).forEach(exports.unlockSync)
})

// XXX https://github.com/joyent/node/issues/3555
// Remove when node 0.8 is deprecated.
process.on('uncaughtException', function H (er) {
  var l = process.listeners('uncaughtException').filter(function (h) {
    return h !== H
  })
  if (!l.length) {
    // cleanup
    try { Object.keys(locks).forEach(exports.unlockSync) } catch (e) {}
    process.removeListener('uncaughtException', H)
    throw er
  }
})

exports.unlock = function (path, cb) {
  // best-effort.  unlocking an already-unlocked lock is a noop
  if (hasOwnProperty(locks, path))
    fs.close(locks[path], unlink)
  else
    unlink()

  function unlink () {
    delete locks[path]
    fs.unlink(path, function (unlinkEr) { cb() })
  }
}

exports.unlockSync = function (path) {
  // best-effort.  unlocking an already-unlocked lock is a noop
  try { fs.closeSync(locks[path]) } catch (er) {}
  try { fs.unlinkSync(path) } catch (er) {}
  delete locks[path]
}


// if the file can be opened in readonly mode, then it's there.
// if the error is something other than ENOENT, then it's not.
exports.check = function (path, opts, cb) {
  if (typeof opts === 'function') cb = opts, opts = {}
  fs.open(path, 'r', function (er, fd) {
    if (er) {
      if (er.code !== 'ENOENT') return cb(er)
      return cb(null, false)
    }

    if (!opts.stale) {
      return fs.close(fd, function (er) {
        return cb(er, true)
      })
    }

    fs.fstat(fd, function (er, st) {
      if (er) return fs.close(fd, function (er2) {
        return cb(er)
      })

      fs.close(fd, function (er) {
        var age = Date.now() - st.ctime.getTime()
        return cb(er, age <= opts.stale)
      })
    })
  })
}

exports.checkSync = function (path, opts) {
  opts = opts || {}
  if (opts.wait) {
    throw new Error('opts.wait not supported sync for obvious reasons')
  }

  try {
    var fd = fs.openSync(path, 'r')
  } catch (er) {
    if (er.code !== 'ENOENT') throw er
    return false
  }

  if (!opts.stale) {
    fs.closeSync(fd)
    return true
  }

  // file exists.  however, might be stale
  if (opts.stale) {
    try {
      var st = fs.fstatSync(fd)
    } finally {
      fs.closeSync(fd)
    }
    var age = Date.now() - st.ctime.getTime()
    return (age <= opts.stale)
  }
}



exports.lock = function (path, opts, cb) {
  if (typeof opts === 'function') cb = opts, opts = {}

  if (typeof opts.retries === 'number' && opts.retries > 0) {
    cb = (function (orig) { return function (er, fd) {
      if (!er) return orig(er, fd)
      var newRT = opts.retries - 1
      opts_ = Object.create(opts, { retries: { value: newRT }})
      if (opts.retryWait) setTimeout(function() {
        exports.lock(path, opts_, orig)
      }, opts.retryWait)
      else exports.lock(path, opts_, orig)
    }})(cb)
  }

  // try to engage the lock.
  // if this succeeds, then we're in business.
  fs.open(path, wx, function (er, fd) {
    if (!er) {
      locks[path] = fd
      return cb(null, fd)
    }

    // something other than "currently locked"
    // maybe eperm or something.
    if (er.code !== 'EEXIST') return cb(er)

    // someone's got this one.  see if it's valid.
    if (opts.stale) fs.stat(path, function (er, st) {
      if (er) {
        if (er.code === 'ENOENT') {
          // expired already!
          var opts_ = Object.create(opts, { stale: { value: false }})
          exports.lock(path, opts_, cb)
          return
        }
        return cb(er)
      }

      var age = Date.now() - st.ctime.getTime()
      if (age > opts.stale) {
        exports.unlock(path, function (er) {
          if (er) return cb(er)
          var opts_ = Object.create(opts, { stale: { value: false }})
          exports.lock(path, opts_, cb)
        })
      } else notStale(er, path, opts, cb)
    })
    else notStale(er, path, opts, cb)
  })
}

function notStale (er, path, opts, cb) {
  // if we can't wait, then just call it a failure
  if (typeof opts.wait !== 'number' || opts.wait <= 0)
    return cb(er)

  // console.error('wait', path, opts.wait)
  // wait for some ms for the lock to clear
  var start = Date.now()
  var end = start + opts.wait

  function retry () {
    var now = Date.now()
    var newWait = end - now
    var newOpts = Object.create(opts, { wait: { value: newWait }})
    exports.lock(path, newOpts, cb)
  }

  var timer = setTimeout(retry, 10)
}

exports.lockSync = function (path, opts) {
  opts = opts || {}
  if (opts.wait || opts.retryWait) {
    throw new Error('opts.wait not supported sync for obvious reasons')
  }

  try {
    var fd = fs.openSync(path, wx)
    locks[path] = fd
    return fd
  } catch (er) {
    if (er.code !== 'EEXIST') return retryThrow(path, opts, er)

    if (opts.stale) {
      var st = fs.statSync(path)
      var age = Date.now() - st.ctime.getTime()
      if (age > opts.stale) {
        exports.unlockSync(path)
        return exports.lockSync(path, opts)
      }
    }

    // failed to lock!
    return retryThrow(path, opts, er)
  }
}

function retryThrow (path, opts, er) {
  if (typeof opts.retries === 'number' && opts.retries > 0) {
    var newRT = opts.retries - 1
    var opts_ = Object.create(opts, { retries: { value: newRT }})
    return exports.lockSync(path, opts_)
  }
  throw er
}

