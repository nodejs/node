'use strict'
var path = require('path')
var fs = require('fs')
var inflight = require('inflight')
var accessError = require('./access-error.js')
var andIgnoreErrors = require('./and-ignore-errors.js')
var isFsAccessAvailable = require('./is-fs-access-available.js')

if (isFsAccessAvailable) {
  module.exports = fsAccessImplementation
} else {
  module.exports = fsOpenImplementation
}

// exposed only for testing purposes
module.exports.fsAccessImplementation = fsAccessImplementation
module.exports.fsOpenImplementation = fsOpenImplementation

function fsAccessImplementation (dir, done) {
  done = inflight('writable:' + dir, done)
  if (!done) return
  fs.access(dir, fs.W_OK, done)
}

function fsOpenImplementation (dir, done) {
  done = inflight('writable:' + dir, done)
  if (!done) return
  var tmp = path.join(dir, '.npm.check.permissions')
  fs.open(tmp, 'w', function (er, fd) {
    if (er) return done(accessError(dir, er))
    fs.close(fd, function () {
      fs.unlink(tmp, andIgnoreErrors(done))
    })
  })
}
