const Arborist = require('../')

const { inspect } = require('util')
const options = require('./lib/options.js')
const print = require('./lib/print-tree.js')
require('./lib/logging.js')
require('./lib/timers.js')

const start = process.hrtime()
new Arborist(options).buildIdealTree(options).then(tree => {
  const end = process.hrtime(start)
  print(tree)
  console.error(`resolved ${tree.inventory.size} deps in ${end[0] + end[1] / 10e9}s`)
  if (tree.meta && options.save) {
    tree.meta.save()
  }
}).catch(er => {
  const opt = { depth: Infinity, color: true }
  console.error(er.code === 'ERESOLVE' ? inspect(er, opt) : er)
  process.exitCode = 1
})
