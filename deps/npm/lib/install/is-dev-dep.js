'use strict'
module.exports = isDevDep

function isDevDep (node, name) {
  return node.package &&
    node.package.devDependencies &&
    node.package.devDependencies[name]
}
