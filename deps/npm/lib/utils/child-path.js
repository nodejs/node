'use strict'
var path = require('path')
var validate = require('aproba')
var moduleName = require('../utils/module-name.js')

module.exports = childPath
function childPath (parentPath, child) {
  validate('SO', arguments)
  return path.join(parentPath, 'node_modules', moduleName(child))
}
