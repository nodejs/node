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

const { resolve } = require('path')
const { homedir } = require('os')
const { depth } = require('treeverse')
const { saveTypeMap } = require('../add-rm-pkg-deps.js')

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
  require('./isolated-reifier.js'),
]

const _workspacesEnabled = Symbol.for('workspacesEnabled')
const Base = mixins.reduce((a, b) => b(a), require('events'))
const getWorkspaceNodes = require('../get-workspace-nodes.js')

// if it's 1, 2, or 3, set it explicitly that.
// if undefined or null, set it null
// otherwise, throw.
const lockfileVersion = lfv => {
  if (lfv === 1 || lfv === 2 || lfv === 3) {
    return lfv
  }

  if (lfv === undefined || lfv === null) {
    return null
  }

  throw new TypeError('Invalid lockfileVersion config: ' + lfv)
}

class Arborist extends Base {
  constructor (options = {}) {
    process.emit('time', 'arborist:ctor')
    super(options)
    this.options = {
      nodeVersion: process.version,
      ...options,
      path: options.path || '.',
      cache: options.cache || `${homedir()}/.npm/_cacache`,
      packumentCache: options.packumentCache || new Map(),
      workspacesEnabled: options.workspacesEnabled !== false,
      replaceRegistryHost: options.replaceRegistryHost,
      lockfileVersion: lockfileVersion(options.lockfileVersion),
      installStrategy: options.global ? 'shallow' : (options.installStrategy ? options.installStrategy : 'hoisted'),
    }
    this.replaceRegistryHost = this.options.replaceRegistryHost =
      (!this.options.replaceRegistryHost || this.options.replaceRegistryHost === 'npmjs') ?
        'registry.npmjs.org' : this.options.replaceRegistryHost

    this[_workspacesEnabled] = this.options.workspacesEnabled

    if (options.saveType && !saveTypeMap.get(options.saveType)) {
      throw new Error(`Invalid saveType ${options.saveType}`)
    }
    this.cache = resolve(this.options.cache)
    this.path = resolve(this.options.path)
    process.emit('timeEnd', 'arborist:ctor')
  }

  // TODO: We should change these to static functions instead
  //   of methods for the next major version

  // returns an array of the actual nodes for all the workspaces
  workspaceNodes (tree, workspaces) {
    return getWorkspaceNodes(tree, workspaces)
  }

  // returns a set of workspace nodes and all their deps
  workspaceDependencySet (tree, workspaces, includeWorkspaceRoot) {
    const wsNodes = this.workspaceNodes(tree, workspaces)
    if (includeWorkspaceRoot) {
      for (const edge of tree.edgesOut.values()) {
        if (edge.type !== 'workspace' && edge.to) {
          wsNodes.push(edge.to)
        }
      }
    }
    const wsDepSet = new Set(wsNodes)
    const extraneous = new Set()
    for (const node of wsDepSet) {
      for (const edge of node.edgesOut.values()) {
        const dep = edge.to
        if (dep) {
          wsDepSet.add(dep)
          if (dep.isLink) {
            wsDepSet.add(dep.target)
          }
        }
      }
      for (const child of node.children.values()) {
        if (child.extraneous) {
          extraneous.add(child)
        }
      }
    }
    for (const extra of extraneous) {
      wsDepSet.add(extra)
    }

    return wsDepSet
  }

  // returns a set of root dependencies, excluding dependencies that are
  // exclusively workspace dependencies
  excludeWorkspacesDependencySet (tree) {
    const rootDepSet = new Set()
    depth({
      tree,
      visit: node => {
        for (const { to } of node.edgesOut.values()) {
          if (!to || to.isWorkspace) {
            continue
          }
          for (const edgeIn of to.edgesIn.values()) {
            if (edgeIn.from.isRoot || rootDepSet.has(edgeIn.from)) {
              rootDepSet.add(to)
            }
          }
        }
        return node
      },
      filter: node => node,
      getChildren: (node, tree) =>
        [...tree.edgesOut.values()].map(edge => edge.to),
    })
    return rootDepSet
  }
}

module.exports = Arborist
