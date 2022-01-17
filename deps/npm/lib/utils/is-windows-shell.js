const isWindows = require('./is-windows.js')
const isWindowsBash = require('./is-windows-bash.js')
module.exports = isWindows && !isWindowsBash
