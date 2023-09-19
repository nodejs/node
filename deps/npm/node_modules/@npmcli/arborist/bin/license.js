const localeCompare = require('@isaacs/string-locale-compare')('en')
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
        throw 'load actual'
      } else {
        return tree
      }
    }).catch((er) => {
      log.error('loading actual tree', er)
      return a.loadActual()
    })
    .then(time)
    .then(({ result: tree }) => {
      const output = []
      if (!query) {
        const set = []
        for (const license of tree.inventory.query('license')) {
          set.push([tree.inventory.query('license', license).size, license])
        }

        for (const [count, license] of set.sort((a, b) =>
          a[1] && b[1] ? b[0] - a[0] || localeCompare(a[1], b[1])
          : a[1] ? -1
          : b[1] ? 1
          : 0)) {
          output.push(`${count} ${license}`)
          log.info(count, license)
        }
      } else {
        for (const node of tree.inventory.query('license', query === 'undefined' ? undefined : query)) {
          const msg = `${node.name} ${node.location} ${node.package.description || ''}`
          output.push(msg)
          log.info(msg)
        }
      }

      return output.join('\n')
    })
}
