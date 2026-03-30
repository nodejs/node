// Dep flag (dev, peer, etc.) calculation requires default or reset flags.
// Flags are true by default and are unset to false as we walk deps.
// We iterate outward edges looking for dep flags that can
// be unset based on the current nodes flags and edge type.
// Examples:
// - a non-optional node with a non-optional edge out, the edge node should not be optional
// - a non-peer node with a non-peer edge out, the edge node should not be peer
// If a node is changed, we add to the queue and continue until no more changes.
// Flags that remain after all this unsetting should be valid.
// Examples:
// - a node still flagged optional must only be reachable via optional edges
// - a node still flagged peer must only be reachable via peer edges
const calcDepFlags = (tree, resetRoot = true) => {
  if (resetRoot) {
    tree.unsetDepFlags()
  }

  const seen = new Set()
  const queue = [tree]

  let node
  while (node = queue.pop()) {
    seen.add(node)

    // Unset extraneous from all parents to avoid removal of children.
    if (!node.extraneous) {
      for (let n = node.resolveParent; n?.extraneous; n = n.resolveParent) {
        n.extraneous = false
      }
    }

    // for links, map their hierarchy appropriately
    if (node.isLink) {
      // node.target can be null, we check to ensure it's not null before proceeding
      if (node.target == null) {
        continue
      }
      node.target.dev = node.dev
      node.target.optional = node.optional
      node.target.devOptional = node.devOptional
      node.target.peer = node.peer
      node.target.extraneous = node.extraneous
      queue.push(node.target)
      continue
    }

    for (const { peer, optional, dev, to } of node.edgesOut.values()) {
      // if the dep is missing, then its flags are already maximally unset
      if (!to) {
        continue
      }

      let changed = false

      // only optional peer dependencies should stay extraneous
      if (to.extraneous && !node.extraneous && !(peer && optional)) {
        to.extraneous = false
        changed = true
      }

      if (to.dev && !node.dev && !dev) {
        to.dev = false
        changed = true
      }

      if (to.optional && !node.optional && !optional) {
        to.optional = false
        changed = true
      }

      // devOptional is the *overlap* of the dev and optional tree.
      // A node may be depended on by separate dev and optional nodes.
      // It SHOULD NOT be removed when pruning dev OR optional.
      // It SHOULD be removed when pruning dev AND optional.
      // We only unset here if a node is not dev AND not optional because
      // if we did unset, it would prevent any overlap deeper in the tree.
      // We correct this later by removing if dev OR optional is set.
      if (to.devOptional && !node.devOptional && !node.dev && !node.optional && !dev && !optional) {
        to.devOptional = false
        changed = true
      }

      if (to.peer && !node.peer && !peer) {
        to.peer = false
        changed = true
      }

      if (changed) {
        queue.push(to)
      }
    }
  }

  // Remove incorrect devOptional flags now that we have walked all deps.
  seen.delete(tree)
  for (const node of seen.values()) {
    if (node.devOptional && (node.dev || node.optional)) {
      node.devOptional = false
    }
  }
}

module.exports = calcDepFlags
