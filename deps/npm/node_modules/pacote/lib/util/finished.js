'use strict'

const BB = require('bluebird')

module.exports = function (child, hasExitCode = false) {
  return BB.fromNode(function (cb) {
    child.on('error', cb)
    child.on(hasExitCode ? 'close' : 'end', function (exitCode) {
      if (exitCode === undefined || exitCode === 0) {
        cb()
      } else {
        let err = new Error('exited with error code: ' + exitCode)
        cb(err)
      }
    })
  })
}
