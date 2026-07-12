const Arborist = require('../')

const log = require('./lib/logging.js')

module.exports = (options, time) => {
  const query = options._.shift()
  const a = new Arborist(options)
  return a
    .loadVirtual()
    .then(tree => {
      // only load the actual tree if the virtual one doesn't have modern metadata
      if (!tree.meta || !(tree.meta.originalLockfileVersion >= 2)) {
        log.error('old metadata, load actual')
        throw 'load actual'
      } else {
        log.error('meta ok, return virtual tree')
        return tree
      }
    })
    .catch(() => a.loadActual())
    .then(time)
    .then(({ timing, result: tree }) => {
      if (!query) {
        for (const node of tree.inventory.values()) {
          if (node.package.funding) {
            log.info(node.name, node.location, node.package.funding)
          }
        }
      } else {
        for (const node of tree.inventory.query('name', query)) {
          if (node.package.funding) {
            log.info(node.name, node.location, node.package.funding)
          }
        }
      }
      return `read ${tree.inventory.size} deps in ${timing.ms}`
    })
}
