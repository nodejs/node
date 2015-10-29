'use strict'
var path = require('path')
var validate = require('aproba')

module.exports = moduleName
module.exports.test = {}

module.exports.test.pathToPackageName = pathToPackageName
function pathToPackageName (dir) {
  if (dir == null) return ''
  if (dir === '') return ''
  var name = path.relative(path.resolve(dir, '..'), dir)
  var scoped = path.relative(path.resolve(dir, '../..'), dir)
  if (scoped[0] === '@') return scoped
  return name
}

module.exports.test.isNotEmpty = isNotEmpty
function isNotEmpty (str) {
  return str != null && str !== ''
}

var unknown = 0
function moduleName (tree) {
  validate('O', arguments)
  var pkg = tree.package || tree
  if (isNotEmpty(pkg.name)) return pkg.name
  var pkgName = pathToPackageName(tree.path)
  if (pkgName !== '') return pkgName
  if (tree._invalidName != null) return tree._invalidName
  tree._invalidName = '!invalid#' + (++unknown)
  return tree._invalidName
}
