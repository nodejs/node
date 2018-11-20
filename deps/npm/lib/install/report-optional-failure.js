'use strict'
var path = require('path')
var moduleName = require('../utils/module-name.js')

module.exports = reportOptionalFailure

function top (tree) {
  if (tree.parent) return top(tree.parent)
  return tree
}

function reportOptionalFailure (tree, what, error) {
  var topTree = top(tree)
  if (!topTree.warnings) topTree.warnings = []
  var id
  if (what) {
    var depVer = tree.package.dependencies && tree.package.dependencies[what]
    var optDepVer = tree.package.optionalDependencies && tree.package.optionalDependencies[what]
    var devDepVer = tree.package.devDependencies && tree.package.devDependencies[what]
    var version = depVer || optDepVer || devDepVer
    id = what + (version ? '@' + version : '')
  } else {
    id = tree._id || moduleName(tree) + (tree.package.version ? '@' + tree.package.version : '')
  }
  var location = path.relative(topTree.path, tree.path)
  if (what) location = path.join(location, 'node_modules', what)

  error.optional = id
  error.location = location
  topTree.warnings.push(error)
}
