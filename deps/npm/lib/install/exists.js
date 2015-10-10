'use strict'
var fs = require('fs')
var inflight = require('inflight')
var accessError = require('./access-error.js')
var isFsAccessAvailable = require('./is-fs-access-available.js')

if (isFsAccessAvailable) {
  module.exports = fsAccessImplementation
} else {
  module.exports = fsStatImplementation
}

// exposed only for testing purposes
module.exports.fsAccessImplementation = fsAccessImplementation
module.exports.fsStatImplementation = fsStatImplementation

function fsAccessImplementation (dir, done) {
  done = inflight('exists:' + dir, done)
  if (!done) return
  fs.access(dir, fs.F_OK, done)
}

function fsStatImplementation (dir, done) {
  done = inflight('exists:' + dir, done)
  if (!done) return
  fs.stat(dir, function (er) { done(accessError(dir, er)) })
}
