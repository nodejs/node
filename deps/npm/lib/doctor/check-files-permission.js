var fs = require('fs')
var path = require('path')
var getUid = require('uid-number')
var chain = require('slide').chain
var log = require('npmlog')
var npm = require('../npm.js')
var fileCompletion = require('../utils/completion/file-completion.js')

function checkFilesPermission (root, mask, cb) {
  if (process.platform === 'win32') return cb(null, true)
  getUid(npm.config.get('user'), npm.config.get('group'), function (e, uid, gid) {
    var tracker = log.newItem('checkFilePermissions', 1)
    if (e) {
      tracker.finish()
      tracker.warn('checkFilePermissions', 'Error looking up user and group:', e)
      return cb(e)
    }
    tracker.info('checkFilePermissions', 'Building file list of ' + root)
    fileCompletion(root, '.', Infinity, function (e, files) {
      if (e) {
        tracker.warn('checkFilePermissions', 'Error building file list:', e)
        tracker.finish()
        return cb(e)
      }
      tracker.addWork(files.length)
      tracker.completeWork(1)
      chain(files.map(andCheckFile), function (er) {
        tracker.finish()
        cb(null, !er)
      })
      function andCheckFile (f) {
        return [checkFile, f]
      }
      function checkFile (f, next) {
        var file = path.join(root, f)
        tracker.silly('checkFilePermissions', f)
        fs.lstat(file, function (e, stat) {
          tracker.completeWork(1)
          if (e) return next(e)
          if (!stat.isFile()) return next()
          // 6 = fs.constants.R_OK | fs.constants.W_OK
          // constants aren't available on v4
          fs.access(file, 6, (err) => {
            if (err) {
              tracker.error('checkFilePermissions', `Missing permissions on ${file}`)
              return next(new Error('Missing permissions for ' + file))
            } else {
              return next()
            }
          })
        })
      }
    })
  })
}

module.exports = checkFilesPermission
