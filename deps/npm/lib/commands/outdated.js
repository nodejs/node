const os = require('os')
const { resolve } = require('path')
const pacote = require('pacote')
const table = require('text-table')
const npa = require('npm-package-arg')
const pickManifest = require('npm-pick-manifest')
const localeCompare = require('@isaacs/string-locale-compare')('en')

const Arborist = require('@npmcli/arborist')

const ansiTrim = require('../utils/ansi-trim.js')
const ArboristWorkspaceCmd = require('../arborist-cmd.js')

class Outdated extends ArboristWorkspaceCmd {
  static description = 'Check for outdated packages'
  static name = 'outdated'
  static usage = ['[<package-spec> ...]']
  static params = [
    'all',
    'json',
    'long',
    'parseable',
    'global',
    'workspace',
  ]

  async exec (args) {
    const global = resolve(this.npm.globalDir, '..')
    const where = this.npm.global
      ? global
      : this.npm.prefix

    const arb = new Arborist({
      ...this.npm.flatOptions,
      path: where,
    })

    this.edges = new Set()
    this.list = []
    this.tree = await arb.loadActual()

    if (this.workspaceNames && this.workspaceNames.length) {
      this.filterSet =
        arb.workspaceDependencySet(
          this.tree,
          this.workspaceNames,
          this.npm.flatOptions.includeWorkspaceRoot
        )
    } else if (!this.npm.flatOptions.workspacesEnabled) {
      this.filterSet =
        arb.excludeWorkspacesDependencySet(this.tree)
    }

    if (args.length !== 0) {
      // specific deps
      for (let i = 0; i < args.length; i++) {
        const nodes = this.tree.inventory.query('name', args[i])
        this.getEdges(nodes, 'edgesIn')
      }
    } else {
      if (this.npm.config.get('all')) {
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
    const outdated = this.list.sort((a, b) => localeCompare(a.name, b.name))

    if (outdated.length > 0) {
      process.exitCode = 1
    }

    // return if no outdated packages
    if (outdated.length === 0 && !this.npm.config.get('json')) {
      return
    }

    // display results
    if (this.npm.config.get('json')) {
      this.npm.output(this.makeJSON(outdated))
    } else if (this.npm.config.get('parseable')) {
      this.npm.output(this.makeParseable(outdated))
    } else {
      const outList = outdated.map(x => this.makePretty(x))
      const outHead = ['Package',
        'Current',
        'Wanted',
        'Latest',
        'Location',
        'Depended by',
      ]

      if (this.npm.config.get('long')) {
        outHead.push('Package Type', 'Homepage')
      }
      const outTable = [outHead].concat(outList)

      outTable[0] = outTable[0].map(heading => this.npm.chalk.underline(heading))

      const tableOpts = {
        align: ['l', 'r', 'r', 'r', 'l'],
        stringLength: s => ansiTrim(s).length,
      }
      this.npm.output(table(outTable, tableOpts))
    }
  }

  getEdges (nodes, type) {
    // when no nodes are provided then it should only read direct deps
    // from the root node and its workspaces direct dependencies
    if (!nodes) {
      this.getEdgesOut(this.tree)
      this.getWorkspacesEdges()
      return
    }

    for (const node of nodes) {
      type === 'edgesOut'
        ? this.getEdgesOut(node)
        : this.getEdgesIn(node)
    }
  }

  getEdgesIn (node) {
    for (const edge of node.edgesIn) {
      this.trackEdge(edge)
    }
  }

  getEdgesOut (node) {
    // TODO: normalize usage of edges and avoid looping through nodes here
    if (this.npm.global) {
      for (const child of node.children.values()) {
        this.trackEdge(child)
      }
    } else {
      for (const edge of node.edgesOut.values()) {
        this.trackEdge(edge)
      }
    }
  }

  trackEdge (edge) {
    const filteredOut =
      edge.from
        && this.filterSet
        && this.filterSet.size > 0
        && !this.filterSet.has(edge.from.target)

    if (filteredOut) {
      return
    }

    this.edges.add(edge)
  }

  getWorkspacesEdges (node) {
    if (this.npm.global) {
      return
    }

    for (const edge of this.tree.edgesOut.values()) {
      const workspace = edge
        && edge.to
        && edge.to.target
        && edge.to.target.isWorkspace

      if (workspace) {
        this.getEdgesOut(edge.to.target)
      }
    }
  }

  async getPackument (spec) {
    const packument = await pacote.packument(spec, {
      ...this.npm.flatOptions,
      fullMetadata: this.npm.config.get('long'),
      preferOnline: true,
    })
    return packument
  }

  async getOutdatedInfo (edge) {
    let alias = false
    try {
      alias = npa(edge.spec).subSpec
    } catch (err) {
      // ignore errors, no alias
    }
    const spec = npa(alias ? alias.name : edge.name)
    const node = edge.to || edge
    const { path, location } = node
    const { version: current } = node.package || {}

    const type = edge.optional ? 'optionalDependencies'
      : edge.peer ? 'peerDependencies'
      : edge.dev ? 'devDependencies'
      : 'dependencies'

    for (const omitType of this.npm.flatOptions.omit) {
      if (node[omitType]) {
        return
      }
    }

    // deps different from prod not currently
    // on disk are not included in the output
    if (edge.error === 'MISSING' && type !== 'dependencies') {
      return
    }

    try {
      const packument = await this.getPackument(spec)
      const expected = alias ? alias.fetchSpec : edge.spec
      // if it's not a range, version, or tag, skip it
      try {
        if (!npa(`${edge.name}@${edge.spec}`).registry) {
          return null
        }
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
        const dependent = edge.from ?
          this.maybeWorkspaceName(edge.from)
          : 'global'

        this.list.push({
          name: alias ? edge.spec.replace('npm', edge.name) : edge.name,
          path,
          type,
          current,
          location,
          wanted: wanted.version,
          latest: latest.version,
          dependent,
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
      ) {
        throw err
      }
    }
  }

  maybeWorkspaceName (node) {
    if (!node.isWorkspace) {
      return node.name
    }

    const humanOutput =
      !this.npm.config.get('json') && !this.npm.config.get('parseable')

    const workspaceName =
      humanOutput
        ? node.pkgid
        : node.name

    return humanOutput
      ? this.npm.chalk.green(workspaceName)
      : workspaceName
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

    if (this.npm.config.get('long')) {
      columns[6] = type
      columns[7] = homepage
    }

    columns[0] = this.npm.chalk[current === wanted ? 'yellow' : 'red'](columns[0]) // current
    columns[2] = this.npm.chalk.green(columns[2]) // wanted
    columns[3] = this.npm.chalk.magenta(columns[3]) // latest

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
      if (this.npm.config.get('long')) {
        out.push(type, homepage)
      }

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
      if (this.npm.config.get('long')) {
        out[name].type = type
        out[name].homepage = homepage
      }
    })
    return JSON.stringify(out, null, 2)
  }
}
module.exports = Outdated
