const { explainNode } = require('./utils/explain-dep.js')
const completion = require('./utils/completion/installed-deep.js')
const Arborist = require('@npmcli/arborist')
const npa = require('npm-package-arg')
const semver = require('semver')
const { relative, resolve } = require('path')
const validName = require('validate-npm-package-name')
const BaseCommand = require('./base-command.js')

class Explain extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'explain'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['<folder | specifier>']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  async completion (opts) {
    return completion(this.npm, opts)
  }

  exec (args, cb) {
    this.explain(args).then(() => cb()).catch(cb)
  }

  async explain (args) {
    if (!args.length)
      throw this.usage

    const arb = new Arborist({ path: this.npm.prefix, ...this.npm.flatOptions })
    const tree = await arb.loadActual()

    const nodes = new Set()
    for (const arg of args) {
      for (const node of this.getNodes(tree, arg))
        nodes.add(node)
    }
    if (nodes.size === 0)
      throw `No dependencies found matching ${args.join(', ')}`

    const expls = []
    for (const node of nodes) {
      const { extraneous, dev, optional, devOptional, peer, inBundle } = node
      const expl = node.explain()
      if (extraneous)
        expl.extraneous = true
      else {
        expl.dev = dev
        expl.optional = optional
        expl.devOptional = devOptional
        expl.peer = peer
        expl.bundled = inBundle
      }
      expls.push(expl)
    }

    if (this.npm.flatOptions.json)
      this.npm.output(JSON.stringify(expls, null, 2))
    else {
      this.npm.output(expls.map(expl => {
        return explainNode(expl, Infinity, this.npm.color)
      }).join('\n\n'))
    }
  }

  getNodes (tree, arg) {
    // if it's just a name, return packages by that name
    const { validForOldPackages: valid } = validName(arg)
    if (valid)
      return tree.inventory.query('name', arg)

    // if it's a location, get that node
    const maybeLoc = arg.replace(/\\/g, '/').replace(/\/+$/, '')
    const nodeByLoc = tree.inventory.get(maybeLoc)
    if (nodeByLoc)
      return [nodeByLoc]

    // maybe a path to a node_modules folder
    const maybePath = relative(this.npm.prefix, resolve(maybeLoc))
      .replace(/\\/g, '/').replace(/\/+$/, '')
    const nodeByPath = tree.inventory.get(maybePath)
    if (nodeByPath)
      return [nodeByPath]

    // otherwise, try to select all matching nodes
    try {
      return this.getNodesByVersion(tree, arg)
    } catch (er) {
      return []
    }
  }

  getNodesByVersion (tree, arg) {
    const spec = npa(arg, this.npm.prefix)
    if (spec.type !== 'version' && spec.type !== 'range')
      return []

    return tree.inventory.filter(node => {
      return node.package.name === spec.name &&
        semver.satisfies(node.package.version, spec.rawSpec)
    })
  }
}
module.exports = Explain
