const _idealTreePrune = Symbol.for('idealTreePrune')

module.exports = cls => class Pruner extends cls {
  async prune (options = {}) {
    // allow the user to set options on the ctor as well.
    // XXX: deprecate separate method options objects.
    options = { ...this.options, ...options }

    await this.buildIdealTree(options)

    this[_idealTreePrune]()

    return this.reify(options)
  }
}
