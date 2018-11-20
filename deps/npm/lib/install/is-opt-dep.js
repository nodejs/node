'use strict'
module.exports = isOptDep

function isOptDep (node, name) {
  return node.package &&
    node.package.optionalDependencies &&
    node.package.optionalDependencies[name]
}
