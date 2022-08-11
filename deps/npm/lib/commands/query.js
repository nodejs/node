'use strict'

const { resolve } = require('path')
const Arborist = require('@npmcli/arborist')
const BaseCommand = require('../base-command.js')

class QuerySelectorItem {
  constructor (node) {
    // all enumerable properties from the target
    Object.assign(this, node.target.package)

    // append extra info
    this.pkgid = node.target.pkgid
    this.location = node.target.location
    this.path = node.target.path
    this.realpath = node.target.realpath
    this.resolved = node.target.resolved
    this.from = []
    this.to = []
    this.dev = node.target.dev
    this.inBundle = node.target.inBundle
    this.deduped = this.from.length > 1
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
  #seen = new Set() // paths we've seen so we can keep response deduped

  static description = 'Retrieve a filtered list of packages'
  static name = 'query'
  static usage = ['<selector>']

  static ignoreImplicitWorkspace = false

  static params = [
    'global',
    'workspace',
    'workspaces',
    'include-workspace-root',
  ]

  get parsedResponse () {
    return JSON.stringify(this.#response, null, 2)
  }

  async exec (args) {
    // one dir up from wherever node_modules lives
    const where = resolve(this.npm.dir, '..')
    const opts = {
      ...this.npm.flatOptions,
      path: where,
      forceActual: true,
    }
    const arb = new Arborist(opts)
    const tree = await arb.loadActual(opts)
    const items = await tree.querySelectorAll(args[0])
    this.buildResponse(items)

    this.npm.output(this.parsedResponse)
  }

  async execWorkspaces (args, filters) {
    await this.setWorkspaces(filters)
    const opts = {
      ...this.npm.flatOptions,
      path: this.npm.prefix,
    }
    const arb = new Arborist(opts)
    const tree = await arb.loadActual(opts)
    for (const workspacePath of this.workspacePaths) {
      let items
      if (workspacePath === tree.root.path) {
        // include-workspace-root
        items = await tree.querySelectorAll(args[0])
      } else {
        const [workspace] = await tree.querySelectorAll(`.workspace:path(${workspacePath})`)
        items = await workspace.target.querySelectorAll(args[0])
      }
      this.buildResponse(items)
    }
    this.npm.output(this.parsedResponse)
  }

  // builds a normalized inventory
  buildResponse (items) {
    for (const node of items) {
      if (!this.#seen.has(node.target.location)) {
        const item = new QuerySelectorItem(node)
        this.#response.push(item)
        this.#seen.add(item.location)
      }
    }
  }
}

module.exports = Query
