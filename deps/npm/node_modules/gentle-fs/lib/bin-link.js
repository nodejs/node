'use strict'
// calls linkIfExists on unix, or cmdShimIfExists on Windows
// reads the cmd shim to ensure it's where we need it to be in the case of
// top level global packages

const readCmdShim = require('read-cmd-shim')
const cmdShim = require('cmd-shim')
const {linkIfExists} = require('./link.js')

const binLink = (from, to, opts, cb) => {
  // just for testing
  const platform = opts._FAKE_PLATFORM_ || process.platform
  if (platform !== 'win32') {
    return linkIfExists(from, to, opts, cb)
  }

  if (!opts.clobberLinkGently ||
      opts.force === true ||
      !opts.gently ||
      typeof opts.gently !== 'string') {
    // easy, just go ahead and delete anything in the way
    return cmdShim.ifExists(from, to, cb)
  }

  // read all three shim targets
  // if any exist, and are not a shim to our gently folder, then
  // exit with a simulated EEXIST error.

  const shimFiles = [
    to,
    to + '.cmd',
    to + '.ps1'
  ]

  // call this once we've checked all three, if we're good
  const done = () => cmdShim.ifExists(from, to, cb)
  const then = times(3, done, cb)
  shimFiles.forEach(to => isClobberable(from, to, opts, then))
}

const times = (n, ok, cb) => {
  let errState = null
  return er => {
    if (!errState) {
      if (er) {
        cb(errState = er)
      } else if (--n === 0) {
        ok()
      }
    }
  }
}

const isClobberable = (from, to, opts, cb) => {
  readCmdShim(to, (er, target) => {
    // either going to get an error, or the target of where this
    // cmd shim points.
    // shim, not in opts.gently: simulate EEXIST
    // not a shim: simulate EEXIST
    // ENOENT: fine, move forward
    // shim in opts.gently: fine
    if (er) {
      switch (er.code) {
        case 'ENOENT':
          // totally fine, nothing there to clobber
          return cb()
        case 'ENOTASHIM':
          // something is there, and it's not one of ours
          return cb(simulateEEXIST(from, to))
        default:
          // would probably fail this way later anyway
          // can't read the file, likely can't write it either
          return cb(er)
      }
    }
    // no error, check the target
    if (target.indexOf(opts.gently) !== 0) {
      return cb(simulateEEXIST(from, to))
    }
    // ok!  it's one of ours.
    return cb()
  })
}

const simulateEEXIST = (from, to) => {
  // simulate the EEXIST we'd get from fs.symlink to the file
  const err = new Error('EEXIST: file already exists, cmd shim \'' +
    from + '\' -> \'' + to + '\'')

  err.code = 'EEXIST'
  err.path = from
  err.dest = to
  return err
}

module.exports = binLink
