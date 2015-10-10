'use strict'
var fs = require('fs')
var semver = require('semver')
var isWindows = process.platform === 'win32'

// fs.access first introduced in node 0.12 / io.js
if (!fs.access) {
  module.exports = false
} else if (!isWindows) {
  // fs.access always works on non-Windows OSes
  module.exports = true
} else {
  // The Windows implementation of `fs.access` has a bug where it will
  // sometimes return access errors all the time for directories, even
  // when access is available. As all we actually test ARE directories, this
  // is a bit of a problem.
  // This was fixed in io.js version 1.5.0
  // As of 2015-07-20, it is still unfixed in node:
  // https://github.com/joyent/node/issues/25657

  module.exports = semver.gte(process.version, '1.5.0')
}
