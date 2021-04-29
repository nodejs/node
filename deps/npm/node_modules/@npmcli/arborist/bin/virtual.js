const Arborist = require('../')

const print = require('./lib/print-tree.js')
const options = require('./lib/options.js')
require('./lib/logging.js')
require('./lib/timers.js')

const start = process.hrtime()
new Arborist(options).loadVirtual().then(tree => {
  const end = process.hrtime(start)
  if (!options.quiet)
    print(tree)
  if (options.save)
    tree.meta.save()
  console.error(`read ${tree.inventory.size} deps in ${end[0] * 1000 + end[1] / 1e6}ms`)
}).catch(er => console.error(er))
