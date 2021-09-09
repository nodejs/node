module.exports = cls => class Deduper extends cls {
  async dedupe (options = {}) {
    // allow the user to set options on the ctor as well.
    // XXX: deprecate separate method options objects.
    options = { ...this.options, ...options }
    const tree = await this.loadVirtual().catch(() => this.loadActual())
    const names = []
    for (const name of tree.inventory.query('name')) {
      if (tree.inventory.query('name', name).size > 1) {
        names.push(name)
      }
    }
    return this.reify({
      ...options,
      preferDedupe: true,
      update: { names },
    })
  }
}
