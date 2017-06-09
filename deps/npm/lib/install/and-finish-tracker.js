'use strict'
var validate = require('aproba')

module.exports = function (tracker, cb) {
  validate('OF', [tracker, cb])
  return function () {
    tracker.finish()
    cb.apply(null, arguments)
  }
}

module.exports.now = function (tracker, cb) {
  validate('OF', [tracker, cb])
  tracker.finish()
  cb.apply(null, Array.prototype.slice.call(arguments, 2))
}
