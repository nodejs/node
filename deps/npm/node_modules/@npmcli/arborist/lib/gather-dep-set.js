// Given a set of nodes in a tree, and a filter function to test
// incoming edges to the dep set that should be ignored otherwise.
//
// find the set of deps that are only depended upon by nodes in the set, or
// their dependencies, or edges that are ignored.
//
// Used when figuring out what to prune when replacing a node with a newer
// version, or when an optional dep fails to install.

const gatherDepSet = (set, edgeFilter) => {
  const deps = new Set(set)

  // add the full set of dependencies.  note that this loop will continue
  // as the deps set increases in size.
  for (const node of deps) {
    for (const edge of node.edgesOut.values()) {
      if (edge.to && edgeFilter(edge))
        deps.add(edge.to)
    }
  }

  // now remove all nodes in the set that have a dependant outside the set
  // if any change is made, then re-check
  // continue until no changes made, or deps set evaporates fully.
  let changed = true
  while (changed === true && deps.size > 0) {
    changed = false
    for (const dep of deps) {
      for (const edge of dep.edgesIn) {
        if (!deps.has(edge.from) && edgeFilter(edge)) {
          changed = true
          deps.delete(dep)
          break
        }
      }
    }
  }

  return deps
}

module.exports = gatherDepSet
