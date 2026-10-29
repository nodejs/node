const isWindows = require('./is-windows.js')
const getPrefix = require('./get-prefix.js')
const getNodeModules = require('./get-node-modules.js')
const { dirname } = require('path')

module.exports = ({ top, path }) =>
  !top ? getNodeModules(path) + '/.bin'
  : isWindows ? getPrefix(path)
  : dirname(getPrefix(path)) + '/bin'
