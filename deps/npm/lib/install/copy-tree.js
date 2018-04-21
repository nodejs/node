'use strict'
var createNode = require('./node.js').create
module.exports = function (tree) {
  return copyTree(tree, {})
}

function copyTree (tree, cache) {
  if (cache[tree.path]) { return cache[tree.path] }
  var newTree = cache[tree.path] = createNode(Object.assign({}, tree))
  copyModuleList(newTree, 'children', cache)
  newTree.children.forEach(function (child) {
    child.parent = newTree
  })
  copyModuleList(newTree, 'requires', cache)
  copyModuleList(newTree, 'requiredBy', cache)
  return newTree
}

function copyModuleList (tree, key, cache) {
  var newList = []
  if (tree[key]) {
    tree[key].forEach(function (child) {
      const copy = copyTree(child, cache)
      if (copy) {
        newList.push(copy)
      }
    })
  }
  tree[key] = newList
}
