// when we have to dupe a set of peer dependencies deeper into the tree in
// order to make room for a dep that would otherwise conflict, we use
// this to get the set of all deps that have to be checked to ensure
// nothing is locking them into the current location.
//
// this is different in its semantics from an "optional set" (ie, the nodes
// that should be removed if an optional dep fails), because in this case,
// we specifically intend to include deps in the peer set that have
// dependants outside the set.
const peerSet = node => {
  const set = new Set([node])
  for (const node of set) {
    for (const edge of node.edgesOut.values()) {
      if (edge.valid && edge.peer && edge.to)
        set.add(edge.to)
    }
    for (const edge of node.edgesIn) {
      if (edge.valid && edge.peer)
        set.add(edge.from)
    }
  }
  return set
}

module.exports = peerSet
