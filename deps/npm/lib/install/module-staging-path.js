'use strict'
var uniqueFilename = require('unique-filename')
var moduleName = require('../utils/module-name.js')

module.exports = moduleStagingPath
function moduleStagingPath (staging, pkg) {
  return uniqueFilename(staging, moduleName(pkg), pkg.realpath)
}
