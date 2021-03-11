const os = require('os')
const path = require('path')
const pacote = require('pacote')
const table = require('text-table')
const color = require('ansicolors')
const styles = require('ansistyles')
const npa = require('npm-package-arg')
const pickManifest = require('npm-pick-manifest')

const Arborist = require('@npmcli/arborist')

const ansiTrim = require('./utils/ansi-trim.js')
const BaseCommand = require('./base-command.js')

class Outdated extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'outdated'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[[<@scope>/]<pkg> ...]']
  }

  exec (args, cb) {
    this.outdated(args).then(() => cb()).catch(cb)
  }

  async outdated (args) {
    this.opts = this.npm.flatOptions

    const global = path.resolve(this.npm.globalDir, '..')
    const where = this.opts.global
      ? global
      : this.npm.prefix

    const arb = new Arborist({
      ...this.opts,
      path: where,
    })

    this.edges = new Set()
    this.list = []
    this.tree = await arb.loadActual()

    if (args.length !== 0) {
      // specific deps
      for (let i = 0; i < args.length; i++) {
        const nodes = this.tree.inventory.query('name', args[i])
        this.getEdges(nodes, 'edgesIn')
      }
    } else {
      if (this.opts.all) {
        // all deps in tree
        const nodes = this.tree.inventory.values()
        this.getEdges(nodes, 'edgesOut')
      }
      // top-level deps
      this.getEdges()
    }

    await Promise.all(Array.from(this.edges).map((edge) => {
      return this.getOutdatedInfo(edge)
    }))

    // sorts list alphabetically
    const outdated = this.list.sort((a, b) => a.name.localeCompare(b.name))

    // return if no outdated packages
    if (outdated.length === 0 && !this.opts.json)
      return

    // display results
    if (this.opts.json)
      this.npm.output(this.makeJSON(outdated))
    else if (this.opts.parseable)
      this.npm.output(this.makeParseable(outdated))
    else {
      const outList = outdated.map(x => this.makePretty(x))
      const outHead = ['Package',
        'Current',
        'Wanted',
        'Latest',
        'Location',
        'Depended by',
      ]

      if (this.opts.long)
        outHead.push('Package Type', 'Homepage')
      const outTable = [outHead].concat(outList)

      if (this.opts.color)
        outTable[0] = outTable[0].map(heading => styles.underline(heading))

      const tableOpts = {
        align: ['l', 'r', 'r', 'r', 'l'],
        stringLength: s => ansiTrim(s).length,
      }
      this.npm.output(table(outTable, tableOpts))
    }
  }

  getEdges (nodes, type) {
    if (!nodes)
      return this.getEdgesOut(this.tree)
    for (const node of nodes) {
      type === 'edgesOut'
        ? this.getEdgesOut(node)
        : this.getEdgesIn(node)
    }
  }

  getEdgesIn (node) {
    for (const edge of node.edgesIn)
      this.edges.add(edge)
  }

  getEdgesOut (node) {
    if (this.opts.global) {
      for (const child of node.children.values())
        this.edges.add(child)
    } else {
      for (const edge of node.edgesOut.values())
        this.edges.add(edge)
    }
  }

  async getPackument (spec) {
    const packument = await pacote.packument(spec, {
      ...this.npm.flatOptions,
      fullMetadata: this.npm.flatOptions.long,
      preferOnline: true,
    })
    return packument
  }

  async getOutdatedInfo (edge) {
    const spec = npa(edge.name)
    const node = edge.to || edge
    const { path, location } = node
    const { version: current } = node.package || {}

    const type = edge.optional ? 'optionalDependencies'
      : edge.peer ? 'peerDependencies'
      : edge.dev ? 'devDependencies'
      : 'dependencies'

    for (const omitType of this.opts.omit || []) {
      if (node[omitType])
        return
    }

    // deps different from prod not currently
    // on disk are not included in the output
    if (edge.error === 'MISSING' && type !== 'dependencies')
      return

    try {
      const packument = await this.getPackument(spec)
      const expected = edge.spec
      // if it's not a range, version, or tag, skip it
      try {
        if (!npa(`${edge.name}@${edge.spec}`).registry)
          return null
      } catch (err) {
        return null
      }
      const wanted = pickManifest(packument, expected, this.npm.flatOptions)
      const latest = pickManifest(packument, '*', this.npm.flatOptions)

      if (
        !current ||
        current !== wanted.version ||
        wanted.version !== latest.version
      ) {
        this.list.push({
          name: edge.name,
          path,
          type,
          current,
          location,
          wanted: wanted.version,
          latest: latest.version,
          dependent: edge.from ? edge.from.name : 'global',
          homepage: packument.homepage,
        })
      }
    } catch (err) {
      // silently catch and ignore ETARGET, E403 &
      // E404 errors, deps are just skipped
      if (!(
        err.code === 'ETARGET' ||
        err.code === 'E403' ||
        err.code === 'E404')
      )
        throw err
    }
  }

  // formatting functions
  makePretty (dep) {
    const {
      current = 'MISSING',
      location = '-',
      homepage = '',
      name,
      wanted,
      latest,
      type,
      dependent,
    } = dep

    const columns = [name, current, wanted, latest, location, dependent]

    if (this.opts.long) {
      columns[6] = type
      columns[7] = homepage
    }

    if (this.opts.color) {
      columns[0] = color[current === wanted ? 'yellow' : 'red'](columns[0]) // current
      columns[2] = color.green(columns[2]) // wanted
      columns[3] = color.magenta(columns[3]) // latest
    }

    return columns
  }

  // --parseable creates output like this:
  // <fullpath>:<name@wanted>:<name@installed>:<name@latest>:<dependedby>
  makeParseable (list) {
    return list.map(dep => {
      const {
        name,
        current,
        wanted,
        latest,
        path,
        dependent,
        type,
        homepage,
      } = dep
      const out = [
        path,
        name + '@' + wanted,
        current ? (name + '@' + current) : 'MISSING',
        name + '@' + latest,
        dependent,
      ]
      if (this.opts.long)
        out.push(type, homepage)

      return out.join(':')
    }).join(os.EOL)
  }

  makeJSON (list) {
    const out = {}
    list.forEach(dep => {
      const {
        name,
        current,
        wanted,
        latest,
        path,
        type,
        dependent,
        homepage,
      } = dep
      out[name] = {
        current,
        wanted,
        latest,
        dependent,
        location: path,
      }
      if (this.opts.long) {
        out[name].type = type
        out[name].homepage = homepage
      }
    })
    return JSON.stringify(out, null, 2)
  }
}
module.exports = Outdated
