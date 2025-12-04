// Sometimes we need to actually do a walk from the root, because you can
// have a cycle of deps that all depend on each other, but no path from root.
// Also, since the ideal tree is loaded from the shrinkwrap, it had extraneous
// flags set false that might now be actually extraneous, and dev/optional
// flags that are also now incorrect.  This method sets all flags to true, so
// we can find the set that is actually extraneous.
module.exports = tree => {
  for (const node of tree.inventory.values()) {
    node.extraneous = true
    node.dev = true
    node.devOptional = true
    node.peer = true
    node.optional = true
  }
}
