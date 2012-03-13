
/**
 * Tiny wrapper around substack's mkdirp module, to add a return value
 * to it. `true` if any directories were created in the process, `false`
 * if the target directory already existed. The `err` is still the first
 * argument in case anything actually wrong happens.
 */

var fs = require('fs')
  , mkdirp_ = require('mkdirp')

module.exports = function mkdirp (path, cb) {
  // first stat() to see if the target exists
  fs.stat(path, function (err) {
    if (err) {
      if (err.code == 'ENOENT') {
        // doesn't exist, mkdirp it
        mkdirp_(path, function (err) {
          if (err) return cb(err)
          cb(err, true)
        })
      } else {
        cb(err)
      }
      return
    }
    cb(err, false)
  })
}
