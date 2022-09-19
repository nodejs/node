const Arborist = require('../')

const printTree = require('./lib/print-tree.js')

module.exports = (options, time) => new Arborist(options)
  .loadActual(options)
  .then(time)
  .then(async ({ timing, result: tree }) => {
    printTree(tree)
    if (options.save) {
      await tree.meta.save()
    }
    if (options.saveHidden) {
      tree.meta.hiddenLockfile = true
      tree.meta.filename = options.path + '/node_modules/.package-lock.json'
      await tree.meta.save()
    }
    return `read ${tree.inventory.size} deps in ${timing.ms}`
  })
