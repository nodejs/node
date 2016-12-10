'use strict'
module.exports = isExtraneous

function isExtraneous (tree) {
  var result = !isNotExtraneous(tree)
  return result
}

function isNotRequired (tree) {
  return tree.requiredBy && tree.requiredBy.length === 0
}

function parentHasNoPjson (tree) {
  return tree.parent && tree.parent.isTop && tree.parent.error
}

function topHasNoPjson (tree) {
  var top = tree
  while (!top.isTop) top = top.parent
  return top.error
}

function isNotExtraneous (tree, isCycle) {
  if (!isCycle) isCycle = {}
  if (tree.isTop || tree.userRequired) {
    return true
  } else if (isNotRequired(tree) && parentHasNoPjson(tree)) {
    return true
  } else if (isCycle[tree.path]) {
    return topHasNoPjson(tree)
  } else {
    isCycle[tree.path] = true
    return tree.requiredBy && tree.requiredBy.some(function (node) {
      return isNotExtraneous(node, Object.create(isCycle))
    })
  }
}
