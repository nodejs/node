'use strict'
var isWindows = require('./is-windows.js')
var isWindowsBash = require('./is-windows-bash.js')
module.exports = isWindows && !isWindowsBash
