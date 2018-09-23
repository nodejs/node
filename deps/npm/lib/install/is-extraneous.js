'use strict'
var path = require('path')
var isDev = require('./is-dev.js').isDev
var npm = require('../npm.js')

module.exports = function (tree) {
  var pkg = tree.package
  var requiredBy = pkg._requiredBy.filter(function (req) { return req[0] !== '#' })
  var isTopLevel = tree.parent == null
  var isChildOfTop = !isTopLevel && tree.parent.parent == null
  var isTopGlobal = isChildOfTop && tree.parent.path === path.resolve(npm.globalDir, '..')
  var topHasNoPackageJson = isChildOfTop && tree.parent.error
  return !isTopLevel && (!isChildOfTop || !topHasNoPackageJson) && !isTopGlobal && requiredBy.length === 0 && !isDev(tree)
}
