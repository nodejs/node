const {dirname} = require('path')
const getNodeModules = require('./get-node-modules.js')
module.exports = path => dirname(getNodeModules(path))
