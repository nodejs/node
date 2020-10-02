// Perform a depth-first walk of a tree, ONLY doing the descent (visit)
//
// This uses a stack rather than recursion, so that it can handle deeply
// nested trees without call stack overflows.  (My kingdom for proper TCO!)
//
// This is only used for cases where leave() is not specified.
//
// a
// +-- b
// |   +-- 1
// |   +-- 2
// +-- c
//     +-- 3
//     +-- 4
//
// Expect:
// visit a
// visit b
// visit 1
// visit 2
// visit c
// visit 3
// visit 4
//
// stack.push(tree)
// while stack not empty
//   pop T from stack
//   VISIT(T)
//   get children C of T
//   push each C onto stack

const depth = ({
  visit,
  filter,
  getChildren,
  tree,
}) => {
  const stack = []
  const seen = new Map()

  const next = () => {
    while (stack.length) {
      const node = stack.pop()
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
    stack.push(...kids)
  }

  stack.push(tree)
  return next()
}

const isPromise = p => p && typeof p.then === 'function'

module.exports = depth
