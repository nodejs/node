'use strict'
var moduleName = require('../utils/module-name.js')

function andIsDev (name) {
  return function (req) {
    return req.package &&
      req.package.devDependencies &&
      req.package.devDependencies[name]
  }
}

exports.isDev = function (node) {
  return node.requiredBy.some(andIsDev(moduleName(node)))
}

function andIsOnlyDev (name) {
  var isThisDev = andIsDev(name)
  return function (req) {
    return isThisDev(req) &&
      (!req.package.dependencies || !req.package.dependencies[name])
  }
}

exports.isOnlyDev = function (node) {
  return node.requiredBy.every(andIsOnlyDev(moduleName(node)))
}
