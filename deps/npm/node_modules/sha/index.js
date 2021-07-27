'use strict'

var Transform = require('stream').Transform
var crypto = require('crypto')
var fs = require('graceful-fs')

exports.check = check
exports.checkSync = checkSync
exports.get = get
exports.getSync = getSync
exports.stream = stream

function check(file, expected, options, cb) {
  if (typeof options === 'function') {
    cb = options
    options = undefined
  }
  expected = expected.toLowerCase().trim()
  get(file, options, function (er, actual) {
    if (er) {
      if (er.message) er.message += ' while getting shasum for ' + file
      return cb(er)
    }
    if (actual === expected) return cb(null)
    cb(new Error(
        'shasum check failed for ' + file + '\n'
      + 'Expected: ' + expected + '\n'
      + 'Actual:   ' + actual))
  })
}
function checkSync(file, expected, options) {
  expected = expected.toLowerCase().trim()
  var actual
  try {
    actual = getSync(file, options)
  } catch (er) {
    if (er.message) er.message += ' while getting shasum for ' + file
    throw er
  }
  if (actual !== expected) {
    var ex = new Error(
        'shasum check failed for ' + file + '\n'
      + 'Expected: ' + expected + '\n'
      + 'Actual:   ' + actual)
    throw ex
  }
}


function get(file, options, cb) {
  if (typeof options === 'function') {
    cb = options
    options = undefined
  }
  options = options || {}
  var algorithm = options.algorithm || 'sha1'
  var hash = crypto.createHash(algorithm)
  var source = fs.createReadStream(file)
  var errState = null
  source
    .on('error', function (er) {
      if (errState) return
      return cb(errState = er)
    })
    .on('data', function (chunk) {
      if (errState) return
      hash.update(chunk)
    })
    .on('end', function () {
      if (errState) return
      var actual = hash.digest("hex").toLowerCase().trim()
      cb(null, actual)
    })
}

function getSync(file, options) {
  options = options || {}
  var algorithm = options.algorithm || 'sha1'
  var hash = crypto.createHash(algorithm)
  var source = fs.readFileSync(file)
  hash.update(source)
  return hash.digest("hex").toLowerCase().trim()
}

function stream(expected, options) {
  expected = expected.toLowerCase().trim()
  options = options || {}
  var algorithm = options.algorithm || 'sha1'
  var hash = crypto.createHash(algorithm)

  var stream = new Transform()
  stream._transform = function (chunk, encoding, callback) {
    hash.update(chunk)
    stream.push(chunk)
    callback()
  }
  stream._flush = function (cb) {
    var actual = hash.digest("hex").toLowerCase().trim()
    if (actual === expected) return cb(null)
    cb(new Error(
        'shasum check failed for:\n'
      + '  Expected: ' + expected + '\n'
      + '  Actual:   ' + actual))
    this.push(null)
  }
  return stream
}