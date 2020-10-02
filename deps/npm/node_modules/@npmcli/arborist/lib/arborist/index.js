// The arborist manages three trees:
// - actual
// - virtual
// - ideal
//
// The actual tree is what's present on disk in the node_modules tree
// and elsewhere that links may extend.
//
// The virtual tree is loaded from metadata (package.json and lock files).
//
// The ideal tree is what we WANT that actual tree to become.  This starts
// with the virtual tree, and then applies the options requesting
// add/remove/update actions.
//
// To reify a tree, we calculate a diff between the ideal and actual trees,
// and then turn the actual tree into the ideal tree by taking the actions
// required.  At the end of the reification process, the actualTree is
// updated to reflect the changes.
//
// Each tree has an Inventory at the root.  Shrinkwrap is tracked by Arborist
// instance.  It always refers to the actual tree, but is updated (and written
// to disk) on reification.

// Each of the mixin "classes" adds functionality, but are not dependent on
// constructor call order.  So, we just load them in an array, and build up
// the base class, so that the overall voltron class is easier to test and
// cover, and separation of concerns can be maintained.

const {resolve} = require('path')
const {homedir} = require('os')

const mixins = [
  require('../tracker.js'),
  require('./pruner.js'),
  require('./deduper.js'),
  require('./audit.js'),
  require('./build-ideal-tree.js'),
  require('./load-workspaces.js'),
  require('./load-actual.js'),
  require('./load-virtual.js'),
  require('./rebuild.js'),
  require('./reify.js'),
]

const Base = mixins.reduce((a, b) => b(a), require('events'))

class Arborist extends Base {
  constructor (options = {}) {
    process.emit('time', 'arborist:ctor')
    super(options)
    this.options = {
      nodeVersion: process.version,
      ...options,
      path: options.path || '.',
      cache: options.cache || `${homedir()}/.npm/_cacache`,
    }
    this.cache = resolve(this.options.cache)
    this.path = resolve(this.options.path)
    process.emit('timeEnd', 'arborist:ctor')
  }
}

module.exports = Arborist
