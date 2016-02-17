'use strict'
var fs = require('graceful-fs')
var SaveStack = require('./save-stack.js')

module.exports = rename

function rename (from, to, cb) {
  var saved = new SaveStack(rename)
  fs.rename(from, to, function (er) {
    if (er) {
      return cb(saved.completeWith(er))
    } else {
      return cb()
    }
  })
}
