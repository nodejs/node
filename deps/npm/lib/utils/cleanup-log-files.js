// module to clean out the old log files in cache/_logs
// this is a best-effort attempt.  if a rm fails, we just
// log a message about it and move on.  We do return a
// Promise that succeeds when we've tried to delete everything,
// just for the benefit of testing this function properly.

const { resolve } = require('path')
const rimraf = require('rimraf')
const glob = require('glob')
module.exports = (cache, max, warn) => {
  return new Promise(done => {
    glob(resolve(cache, '_logs', '*-debug.log'), (er, files) => {
      if (er) {
        return done()
      }

      let pending = files.length - max
      if (pending <= 0) {
        return done()
      }

      for (let i = 0; i < files.length - max; i++) {
        rimraf(files[i], er => {
          if (er) {
            warn('log', 'failed to remove log file', files[i])
          }

          if (--pending === 0) {
            done()
          }
        })
      }
    })
  })
}
