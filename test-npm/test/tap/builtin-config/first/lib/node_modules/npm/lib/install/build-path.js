'use strict'
var uniqueFilename = require('unique-filename')
var moduleName = require('../utils/module-name.js')

module.exports = buildPath
function buildPath (staging, pkg) {
  return uniqueFilename(staging, moduleName(pkg), pkg.realpath)
}
