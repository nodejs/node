'use strict'

module.exports = isProdDep

function isProdDep (node, name) {
  return node.package &&
    node.package.dependencies &&
    node.package.dependencies[name]
}
