var fs = require('fs')

var wx = 'wx'
if (process.version.match(/^v0\.[0-6]/)) {
  var c = require('constants')
  wx = c.O_TRUNC | c.O_CREAT | c.O_WRONLY | c.O_EXCL
}

var debug
var util = require('util')
if (util.debuglog)
  debug = util.debuglog('LOCKFILE')
else if (/\blockfile\b/i.test(process.env.NODE_DEBUG))
  debug = function() {
    var msg = util.format.apply(util, arguments)
    console.error('LOCKFILE %d %s', process.pid, msg)
  }
else
  debug = function() {}

var locks = {}

function hasOwnProperty (obj, prop) {
  return Object.prototype.hasOwnProperty.call(obj, prop)
}

process.on('exit', function () {
  debug('exit listener')
  // cleanup
  Object.keys(locks).forEach(exports.unlockSync)
})

// XXX https://github.com/joyent/node/issues/3555
// Remove when node 0.8 is deprecated.
if (/^v0\.[0-8]/.test(process.version)) {
  debug('uncaughtException, version = %s', process.version)
  process.on('uncaughtException', function H (er) {
    debug('uncaughtException')
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
}

exports.unlock = function (path, cb) {
  debug('unlock', path)
  // best-effort.  unlocking an already-unlocked lock is a noop
  delete locks[path]
  fs.unlink(path, function (unlinkEr) { cb() })
}

exports.unlockSync = function (path) {
  debug('unlockSync', path)
  // best-effort.  unlocking an already-unlocked lock is a noop
  try { fs.unlinkSync(path) } catch (er) {}
  delete locks[path]
}


// if the file can be opened in readonly mode, then it's there.
// if the error is something other than ENOENT, then it's not.
exports.check = function (path, opts, cb) {
  if (typeof opts === 'function') cb = opts, opts = {}
  debug('check', path, opts)
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
  debug('checkSync', path, opts)
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
    try { fs.closeSync(fd) } catch (er) {}
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



var req = 0
exports.lock = function (path, opts, cb) {
  if (typeof opts === 'function') cb = opts, opts = {}
  opts.req = opts.req || req++
  debug('lock', path, opts)

  if (typeof opts.retries === 'number' && opts.retries > 0) {
    cb = (function (orig) { return function (er, fd) {
      if (!er) return orig(er, fd)
      var newRT = opts.retries - 1
      opts_ = Object.create(opts, { retries: { value: newRT }})
      debug('lock retry', path, newRT)
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
      debug('locked', path, fd)
      locks[path] = fd
      return fs.close(fd, function () {
        return cb()
      })
    }

    // something other than "currently locked"
    // maybe eperm or something.
    if (er.code !== 'EEXIST') return cb(er)

    // someone's got this one.  see if it's valid.
    if (opts.stale) fs.stat(path, function (statEr, st) {
      if (statEr) {
        if (statEr.code === 'ENOENT') {
          // expired already!
          var opts_ = Object.create(opts, { stale: { value: false }})
          debug('lock stale enoent retry', path, opts_)
          exports.lock(path, opts_, cb)
          return
        }
        return cb(statEr)
      }

      var age = Date.now() - st.ctime.getTime()
      if (age > opts.stale) {
        debug('lock stale', path, opts_)
        exports.unlock(path, function (er) {
          if (er) return cb(er)
          var opts_ = Object.create(opts, { stale: { value: false }})
          debug('lock stale retry', path, opts_)
          exports.lock(path, opts_, cb)
        })
      } else notStale(er, path, opts, cb)
    })
    else notStale(er, path, opts, cb)
  })
}

function notStale (er, path, opts, cb) {
  debug('notStale', path, opts)

  // if we can't wait, then just call it a failure
  if (typeof opts.wait !== 'number' || opts.wait <= 0)
    return cb(er)

  // console.error('wait', path, opts.wait)
  // wait for some ms for the lock to clear
  var start = Date.now()
  var end = start + opts.wait

  function retry () {
    debug('notStale retry', path, opts)
    var now = Date.now()
    var newWait = end - now
    var newOpts = Object.create(opts, { wait: { value: newWait }})
    exports.lock(path, newOpts, cb)
  }

  var timer = setTimeout(retry, 100)
}

exports.lockSync = function (path, opts) {
  opts = opts || {}
  opts.req = opts.req || req++
  debug('lockSync', path, opts)
  if (opts.wait || opts.retryWait) {
    throw new Error('opts.wait not supported sync for obvious reasons')
  }

  try {
    var fd = fs.openSync(path, wx)
    locks[path] = fd
    try { fs.closeSync(fd) } catch (er) {}
    debug('locked sync!', path, fd)
    return
  } catch (er) {
    if (er.code !== 'EEXIST') return retryThrow(path, opts, er)

    if (opts.stale) {
      var st = fs.statSync(path)
      var ct = st.ctime.getTime()
      if (!(ct % 1000) && (opts.stale % 1000)) {
        // probably don't have subsecond resolution.
        // round up the staleness indicator.
        // Yes, this will be wrong 1/1000 times on platforms
        // with subsecond stat precision, but that's acceptable
        // in exchange for not mistakenly removing locks on
        // most other systems.
        opts.stale = 1000 * Math.ceil(opts.stale / 1000)
      }
      var age = Date.now() - ct
      if (age > opts.stale) {
        debug('lockSync stale', path, opts, age)
        exports.unlockSync(path)
        return exports.lockSync(path, opts)
      }
    }

    // failed to lock!
    debug('failed to lock', path, opts, er)
    return retryThrow(path, opts, er)
  }
}

function retryThrow (path, opts, er) {
  if (typeof opts.retries === 'number' && opts.retries > 0) {
    var newRT = opts.retries - 1
    debug('retryThrow', path, opts, newRT)
    var opts_ = Object.create(opts, { retries: { value: newRT }})
    return exports.lockSync(path, opts_)
  }
  throw er
}

