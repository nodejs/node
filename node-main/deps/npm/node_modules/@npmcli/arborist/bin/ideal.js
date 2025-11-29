const Arborist = require('../')

const printTree = require('./lib/print-tree.js')

module.exports = (options, time) => new Arborist(options)
  .buildIdealTree(options)
  .then(time)
  .then(async ({ timing, result: tree }) => {
    printTree(tree)
    if (tree.meta && options.save) {
      await tree.meta.save()
    }
    return `resolved ${tree.inventory.size} deps in ${timing.seconds}`
  })
