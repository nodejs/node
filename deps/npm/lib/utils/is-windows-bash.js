'use strict'
var isWindows = require('./is-windows.js')
module.exports = isWindows && /^MINGW(32|64)$/.test(process.env.MSYSTEM)
