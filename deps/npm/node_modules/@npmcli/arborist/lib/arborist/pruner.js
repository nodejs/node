const _idealTreePrune = Symbol.for('idealTreePrune')
const _workspacesEnabled = Symbol.for('workspacesEnabled')
const _addNodeToTrashList = Symbol.for('addNodeToTrashList')

module.exports = cls => class Pruner extends cls {
  async prune (options = {}) {
    // allow the user to set options on the ctor as well.
    // XXX: deprecate separate method options objects.
    options = { ...this.options, ...options }

    await this.buildIdealTree(options)

    this[_idealTreePrune]()

    if (!this[_workspacesEnabled]) {
      const excludeNodes = this.excludeWorkspacesDependencySet(this.idealTree)
      for (const node of this.idealTree.inventory.values()) {
        if (
          node.parent !== null
          && !node.isProjectRoot
          && !excludeNodes.has(node)
        ) {
          this[_addNodeToTrashList](node)
        }
      }
    }

    return this.reify(options)
  }
}
