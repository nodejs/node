'use strict'
var createNode = require('./node.js').create
module.exports = function (tree, filter) {
  return copyTree(tree, {}, filter)
}

function copyTree (tree, cache, filter) {
  if (filter && !filter(tree)) { return null }
  if (cache[tree.path]) { return cache[tree.path] }
  var newTree = cache[tree.path] = createNode(Object.assign({}, tree))
  copyModuleList(newTree, 'children', cache, filter)
  newTree.children.forEach(function (child) {
    child.parent = newTree
  })
  copyModuleList(newTree, 'requires', cache, filter)
  copyModuleList(newTree, 'requiredBy', cache, filter)
  return newTree
}

function copyModuleList (tree, key, cache, filter) {
  var newList = []
  if (tree[key]) {
    tree[key].forEach(function (child) {
      const copy = copyTree(child, cache, filter)
      if (copy) {
        newList.push(copy)
      }
    })
  }
  tree[key] = newList
}
