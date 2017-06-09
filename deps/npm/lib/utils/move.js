'use strict'
module.exports = wrappedMove

var fs = require('graceful-fs')
var move = require('move-concurrently')
var Bluebird = require('bluebird')

function wrappedMove (from, to, cb) {
  var movePromise = move(from, to, {fs: fs, Promise: Bluebird, maxConcurrency: 4})
  if (cb) {
    return movePromise.then(function (value) {
      cb(value)
    }, function (err) {
      cb(err)
    })
  } else {
    return movePromise
  }
}
