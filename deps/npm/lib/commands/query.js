const { resolve } = require('node:path')
const BaseCommand = require('../base-cmd.js')
const { log, output } = require('proc-log')

// Ranks competing representations of the same physical package so the most logical one is reported.
// A top-level placement (e.g. node_modules/<pkg>) beats the canonical store node, which beats an internal store symlink.
const locationRank = (node) => {
  if (!node.location.includes('node_modules/.store/')) {
    return 2
  }
  return node.isLink ? 0 : 1
}

class QuerySelectorItem {
  constructor (node) {
    // all enumerable properties from the target
    Object.assign(this, node.target.package)

    // append extra info
    this.pkgid = node.target.pkgid
    // For a dep symlinked into the isolated store, report the logical link location (node_modules/<pkg>) rather than the .store backing path.
    // Workspaces and regular nodes keep the target location (e.g. packages/<ws>).
    const logical = node.target.isInStore ? node : node.target
    this.location = logical.location
    this.path = logical.path
    this.realpath = node.target.realpath
    this.resolved = node.target.resolved
    this.from = []
    this.to = []
    this.dev = node.target.dev
    this.inBundle = node.target.inBundle
    this.deduped = this.from.length > 1
    this.overridden = node.overridden
    this.queryContext = node.queryContext
    for (const edge of node.target.edgesIn) {
      this.from.push(edge.from.location)
    }
    for (const [, edge] of node.target.edgesOut) {
      if (edge.to) {
        this.to.push(edge.to.location)
      }
    }
  }
}

class Query extends BaseCommand {
  #response = [] // response is the query response
  #seen = new Map() // physical location -> index in #response, to keep response deduped

  static description = 'Retrieve a filtered list of packages'
  static name = 'query'
  static usage = ['<selector>']

  static workspaces = true
  static ignoreImplicitWorkspace = false

  static params = [
    'global',
    'workspace',
    'workspaces',
    'include-workspace-root',
    'package-lock-only',
    'expect-results',
    'before',
    'min-release-age',
    'min-release-age-exclude',
  ]

  constructor (...args) {
    super(...args)
    this.npm.config.set('json', true)
  }

  async exec (args) {
    const packageLockOnly = this.npm.config.get('package-lock-only')
    const Arborist = require('@npmcli/arborist')
    const arb = new Arborist({
      ...this.npm.flatOptions,
      // one dir up from wherever node_modules lives
      path: resolve(this.npm.dir, '..'),
      forceActual: !packageLockOnly,
    })
    let tree
    if (packageLockOnly) {
      try {
        tree = await arb.loadVirtual()
      } catch (err) {
        log.verbose('loadVirtual', err.stack)
        throw this.usageError(
          'A package-lock.json file is required in package-lock-only mode'
        )
      }
    } else {
      tree = await arb.loadActual()
    }
    await this.#queryTree(tree, args[0])
    this.#output()
  }

  async execWorkspaces (args) {
    await this.setWorkspaces()
    const packageLockOnly = this.npm.config.get('package-lock-only')
    const Arborist = require('@npmcli/arborist')
    const arb = new Arborist({
      ...this.npm.flatOptions,
      path: this.npm.prefix,
      forceActual: !packageLockOnly,
    })
    let tree
    if (packageLockOnly) {
      try {
        tree = await arb.loadVirtual()
      } catch (err) {
        log.verbose('loadVirtual', err.stack)
        throw this.usageError(
          'A package-lock.json file is required in package-lock-only mode'
        )
      }
    } else {
      tree = await arb.loadActual()
    }
    for (const path of this.workspacePaths) {
      const wsTree = path === tree.root.path
        ? tree // --includes-workspace-root
        : await tree.querySelectorAll(`.workspace:path(${path})`).then(r => r[0]?.target)
      if (wsTree) {
        await this.#queryTree(wsTree, args[0])
      }
    }
    this.#output()
  }

  #output () {
    this.checkExpected(this.#response.length)
    output.buffer(this.#response)
  }

  // builds a normalized inventory
  async #queryTree (tree, arg) {
    const items = await tree.querySelectorAll(arg, this.npm.flatOptions)
    for (const node of items) {
      // Dedup by the target's physical location so multiple logical links to the same store node collapse to one result.
      const { location } = node.target
      if (!location) {
        this.#response.push(new QuerySelectorItem(node))
        continue
      }
      const seen = this.#seen.get(location)
      if (seen === undefined) {
        this.#seen.set(location, { index: this.#response.length, rank: locationRank(node) })
        this.#response.push(new QuerySelectorItem(node))
      } else {
        // Replace the stored representation only with a more logical one for the same physical package.
        const rank = locationRank(node)
        if (rank > seen.rank) {
          this.#response[seen.index] = new QuerySelectorItem(node)
          seen.rank = rank
        }
      }
    }
  }
}

module.exports = Query
