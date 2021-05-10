const Arborist = require('../')
const options = require('./lib/options.js')
require('./lib/logging.js')
require('./lib/timers.js')

const a = new Arborist(options)
const query = options._.shift()

a.loadVirtual().then(tree => {
  // only load the actual tree if the virtual one doesn't have modern metadata
  if (!tree.meta || !(tree.meta.originalLockfileVersion >= 2))
    throw 'load actual'
  else
    return tree
}).catch((er) => {
  console.error('loading actual tree', er)
  return a.loadActual()
}).then(tree => {
  if (!query) {
    const set = []
    for (const license of tree.inventory.query('license'))
      set.push([tree.inventory.query('license', license).size, license])

    for (const [count, license] of set.sort((a, b) =>
      a[1] && b[1] ? b[0] - a[0] || a[1].localeCompare(b[1], 'en')
      : a[1] ? -1
      : b[1] ? 1
      : 0))
      console.log(count, license)
  } else {
    for (const node of tree.inventory.query('license', query === 'undefined' ? undefined : query))
      console.log(`${node.name} ${node.location} ${node.package.description || ''}`)
  }
})
