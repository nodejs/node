// given a starting node, what is the *deepest* target where name could go?
// This is not on the Node class for the simple reason that we sometimes
// need to check the deepest *potential* target for a Node that is not yet
// added to the tree where we are checking.
const deepestNestingTarget = (start, name) => {
  for (const target of start.ancestry()) {
    // note: this will skip past the first target if edge is peer
    if (target.isProjectRoot || !target.resolveParent)
      return target
    const targetEdge = target.edgesOut.get(name)
    if (!targetEdge || !targetEdge.peer)
      return target
  }
}

module.exports = deepestNestingTarget
