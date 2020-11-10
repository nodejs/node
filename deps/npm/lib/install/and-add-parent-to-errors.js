'use strict'
var moduleName = require('../utils/module-name.js')
var validate = require('aproba')

module.exports = function (parent, cb) {
  validate('F', [cb])
  return function (er) {
    if (!er) return cb.apply(null, arguments)
    if (er instanceof Error && parent && parent.package && parent.package.name) {
      er.parent = moduleName(parent)
    }
    cb(er)
  }
}
