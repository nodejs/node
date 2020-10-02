// Perform a breadth-first walk of a tree, either logical or physical
// This one only visits, it doesn't leave.  That's because
// in a breadth-first traversal, children may be visited long
// after their parent, so the "exit" pass ends up being just
// another breadth-first walk.
//
// Breadth-first traversals are good for either creating a tree (ie,
// reifying a dep graph based on a package.json without a node_modules
// or package-lock), or mutating it in-place.  For a map-reduce type of
// walk, it doesn't make a lot of sense, and is very expensive.
const breadth = ({
  visit,
  filter = () => true,
  getChildren,
  tree,
}) => {
  const queue = []
  const seen = new Map()

  const next = () => {
    while (queue.length) {
      const node = queue.shift()
      const res = visitNode(node)
      if (isPromise(res)) {
        return res.then(() => next())
      }
    }
    return seen.get(tree)
  }

  const visitNode = (tree) => {
    if (seen.has(tree))
      return seen.get(tree)

    seen.set(tree, null)
    const res = visit ? visit(tree) : tree
    if (isPromise(res)) {
      const fullResult = res.then(res => {
        seen.set(tree, res)
        return kidNodes(tree)
      })
      seen.set(tree, fullResult)
      return fullResult
    } else {
      seen.set(tree, res)
      return kidNodes(tree)
    }
  }

  const kidNodes = (tree) => {
    const kids = getChildren(tree, seen.get(tree))
    return isPromise(kids) ? kids.then(processKids) : processKids(kids)
  }

  const processKids = (kids) => {
    kids = (kids || []).filter(filter)
    queue.push(...kids)
  }

  queue.push(tree)
  return next()
}

const isPromise = p => p && typeof p.then === 'function'

module.exports = breadth
