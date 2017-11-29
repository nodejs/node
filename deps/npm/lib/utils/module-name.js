'use strict'
var path = require('path')

module.exports = moduleName
module.exports.test = {}

module.exports.test.pathToPackageName = pathToPackageName
function pathToPackageName (dir) {
  if (dir == null) return ''
  if (dir === '') return ''
  var name = path.relative(path.resolve(dir, '..'), dir)
  var scoped = path.relative(path.resolve(dir, '../..'), dir)
  if (scoped[0] === '@') return scoped.replace(/\\/g, '/')
  return name.trim()
}

module.exports.test.isNotEmpty = isNotEmpty
function isNotEmpty (str) {
  return str != null && str !== ''
}

var unknown = 0
function moduleName (tree) {
  var pkg = tree.package || tree
  if (isNotEmpty(pkg.name) && typeof pkg.name === 'string') return pkg.name.trim()
  var pkgName = pathToPackageName(tree.path)
  if (pkgName !== '') return pkgName
  if (tree._invalidName != null) return tree._invalidName
  tree._invalidName = '!invalid#' + (++unknown)
  return tree._invalidName
}
