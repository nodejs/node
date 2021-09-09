// Given a node in a tree, return all of the peer dependency sets that
// it is a part of, with the entry (top or non-peer) edges into the sets
// identified.
//
// With this information, we can determine whether it is appropriate to
// replace the entire peer set with another (and remove the old one),
// push the set deeper into the tree, and so on.
//
// Returns a Map of { edge => Set(peerNodes) },

const peerEntrySets = node => {
  // this is the union of all peer groups that the node is a part of
  // later, we identify all of the entry edges, and create a set of
  // 1 or more overlapping sets that this node is a part of.
  const unionSet = new Set([node])
  for (const node of unionSet) {
    for (const edge of node.edgesOut.values()) {
      if (edge.valid && edge.peer && edge.to) {
        unionSet.add(edge.to)
      }
    }
    for (const edge of node.edgesIn) {
      if (edge.valid && edge.peer) {
        unionSet.add(edge.from)
      }
    }
  }
  const entrySets = new Map()
  for (const peer of unionSet) {
    for (const edge of peer.edgesIn) {
      // if not valid, it doesn't matter anyway.  either it's been previously
      // overridden, or it's the thing we're interested in replacing.
      if (!edge.valid) {
        continue
      }
      // this is the entry point into the peer set
      if (!edge.peer || edge.from.isTop) {
        // get the subset of peer brought in by this peer entry edge
        const sub = new Set([peer])
        for (const peer of sub) {
          for (const edge of peer.edgesOut.values()) {
            if (edge.valid && edge.peer && edge.to) {
              sub.add(edge.to)
            }
          }
        }
        // if this subset does not include the node we are focused on,
        // then it is not relevant for our purposes.  Example:
        //
        // a -> (b, c, d)
        // b -> PEER(d) b -> d -> e -> f <-> g
        // c -> PEER(f, h) c -> (f <-> g, h -> g)
        // d -> PEER(e) d -> e -> f <-> g
        // e -> PEER(f)
        // f -> PEER(g)
        // g -> PEER(f)
        // h -> PEER(g)
        //
        // The unionSet(e) will include c, but we don't actually care about
        // it.  We only expanded to the edge of the peer nodes in order to
        // find the entry edges that caused the inclusion of peer sets
        // including (e), so we want:
        //   Map{
        //     Edge(a->b) => Set(b, d, e, f, g)
        //     Edge(a->d) => Set(d, e, f, g)
        //   }
        if (sub.has(node)) {
          entrySets.set(edge, sub)
        }
      }
    }
  }

  return entrySets
}

module.exports = peerEntrySets
