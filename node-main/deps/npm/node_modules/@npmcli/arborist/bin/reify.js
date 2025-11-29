const Arborist = require('../')

const printTree = require('./lib/print-tree.js')
const log = require('./lib/logging.js')

const printDiff = diff => {
  const { depth } = require('treeverse')
  depth({
    tree: diff,
    visit: d => {
      if (d.location === '') {
        return
      }
      switch (d.action) {
        case 'REMOVE':
          log.info('REMOVE', d.actual.location)
          break
        case 'ADD':
          log.info('ADD', d.ideal.location, d.ideal.resolved)
          break
        case 'CHANGE':
          log.info('CHANGE', d.actual.location, {
            from: d.actual.resolved,
            to: d.ideal.resolved,
          })
          break
      }
    },
    getChildren: d => d.children,
  })
}

module.exports = (options, time) => {
  const arb = new Arborist(options)
  return arb
    .reify(options)
    .then(time)
    .then(async ({ timing, result: tree }) => {
      printTree(tree)
      if (options.dryRun) {
        printDiff(arb.diff)
      }
      if (tree.meta && options.save) {
        await tree.meta.save()
      }
      return `resolved ${tree.inventory.size} deps in ${timing.seconds}`
    })
}
