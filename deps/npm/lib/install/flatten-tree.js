'use strict'
var validate = require('aproba')
var moduleName = require('../utils/module-name.js')

module.exports = flattenTree
module.exports.flatName = flatName
module.exports.flatNameFromTree = flatNameFromTree

function flattenTree (tree) {
  validate('O', arguments)
  var seen = new Set()
  var flat = {}
  var todo = [[tree, '/']]
  while (todo.length) {
    var next = todo.shift()
    var pkg = next[0]
    seen.add(pkg)
    var path = next[1]
    flat[path] = pkg
    if (path !== '/') path += '/'
    for (var ii = 0; ii < pkg.children.length; ++ii) {
      var child = pkg.children[ii]
      if (!seen.has(child)) {
        todo.push([child, flatName(path, child)])
      }
    }
  }
  return flat
}

function flatName (path, child) {
  validate('SO', arguments)
  return path + (moduleName(child) || 'TOP')
}

function flatNameFromTree (tree) {
  validate('O', arguments)
  if (tree.isTop) return '/'
  var path = flatNameFromTree(tree.parent)
  if (path !== '/') path += '/'
  return flatName(path, tree)
}
