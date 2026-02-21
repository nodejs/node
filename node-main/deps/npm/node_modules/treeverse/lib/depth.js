// Perform a depth-first walk of a tree.
//
// `visit(node)` is called when the node is first encountered.
// `leave(node, children)` is called when all of the node's children
// have been left or (in the case of cyclic graphs) visited.
//
// Only one of visit or leave is required.  (Technically both are optional,
// but if you don't provide at least one, the tree is just walked without
// doing anything, which is a bit pointless.)  If visit is provided, and
// leave is not, then this is a root->leaf traversal.  If leave is provided,
// and visit is not, then it's leaf->root.  Both can be provided for a
// map-reduce operation.
//
// If either visit or leave return a Promise for any node, then the
// walk returns a Promise.

const depthDescent = require('./depth-descent.js')
const depth = ({
  visit,
  leave,
  filter = () => true,
  seen = new Map(),
  getChildren,
  tree,
}) => {
  if (!leave) {
    return depthDescent({ visit, filter, getChildren, tree })
  }

  if (seen.has(tree)) {
    return seen.get(tree)
  }

  seen.set(tree, null)

  const visitNode = () => {
    const res = visit ? visit(tree) : tree
    if (isPromise(res)) {
      const fullResult = res.then(resThen => {
        seen.set(tree, resThen)
        return kidNodes()
      })
      seen.set(tree, fullResult)
      return fullResult
    } else {
      seen.set(tree, res)
      return kidNodes()
    }
  }

  const kidNodes = () => {
    const kids = getChildren(tree, seen.get(tree))
    return isPromise(kids) ? kids.then(processKids) : processKids(kids)
  }

  const processKids = nodes => {
    const kids = (nodes || []).filter(filter).map(kid =>
      depth({ visit, leave, filter, seen, getChildren, tree: kid }))
    return kids.some(isPromise)
      ? Promise.all(kids).then(leaveNode)
      : leaveNode(kids)
  }

  const leaveNode = kids => {
    const res = leave(seen.get(tree), kids)
    seen.set(tree, res)
    // if it's a promise at this point, the caller deals with it
    return res
  }

  return visitNode()
}

const isPromise = p => p && typeof p.then === 'function'

module.exports = depth
