'use strict'
var uniqueFilename = require('unique-filename')
var npm = require('../npm.js')

module.exports = function (prefix) {
  return uniqueFilename(npm.tmp, prefix)
}
