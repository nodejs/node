'use strict'
var path = require('path')

module.exports = function (tree) {
  var pkg = tree.package || tree
  // FIXME: Excluding the '@' here is cleaning up after the mess that
  // read-package-json makes. =(
  if (pkg._id && pkg._id !== '@') return pkg._id
  var name = pkg.name || (tree && tree.logical && path.basename(tree.logical))
  if (name && pkg.version) {
    return name + '@' + pkg.version
  } else if (tree) {
    return tree.path
  } else if (name) {
    return name
  } else {
    return ''
  }
}
