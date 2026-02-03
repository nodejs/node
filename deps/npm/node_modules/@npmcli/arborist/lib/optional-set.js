// when an optional dep fails to install, we need to remove the branch of the
// graph up to the first optionalDependencies, as well as any nodes that are
// only required by other nodes in the set.
//
// This function finds the set of nodes that will need to be removed in that
// case.
//
// Note that this is *only* going to work with trees where calcDepFlags
// has been called, because we rely on the node.optional flag.

const gatherDepSet = require('./gather-dep-set.js')
const optionalSet = node => {
  // start with the node, then walk up the dependency graph until we
  // get to the boundaries that define the optional set.  since the
  // node is optional, we know that all paths INTO this area of the
  // graph are optional, but there may be non-optional dependencies
  // WITHIN the area.
  const set = new Set([node])
  for (const node of set) {
    for (const edge of node.edgesIn) {
      if (!edge.optional) {
        set.add(edge.from)
      }
    }
  }

  // now that we've hit the boundary, gather the rest of the nodes in
  // the optional section that don't have dependents outside the set.
  return gatherDepSet(set, edge => !set.has(edge.to))
}

module.exports = optionalSet
