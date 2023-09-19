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

  const visitNode = (visitTree) => {
    if (seen.has(visitTree)) {
      return seen.get(visitTree)
    }

    seen.set(visitTree, null)
    const res = visit ? visit(visitTree) : visitTree
    if (isPromise(res)) {
      const fullResult = res.then(resThen => {
        seen.set(visitTree, resThen)
        return kidNodes(visitTree)
      })
      seen.set(visitTree, fullResult)
      return fullResult
    } else {
      seen.set(visitTree, res)
      return kidNodes(visitTree)
    }
  }

  const kidNodes = (kidTree) => {
    const kids = getChildren(kidTree, seen.get(kidTree))
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
