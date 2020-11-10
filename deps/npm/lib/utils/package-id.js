'use strict'
var moduleName = require('./module-name.js')

module.exports = function (tree) {
  var pkg = tree.package || tree
  // FIXME: Excluding the '@' here is cleaning up after the mess that
  // read-package-json makes. =(
  if (pkg._id && pkg._id !== '@') return pkg._id
  var name = moduleName(tree)
  if (pkg.version) {
    return name + '@' + pkg.version
  } else {
    return name
  }
}
