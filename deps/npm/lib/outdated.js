const os = require('os')
const path = require('path')
const pacote = require('pacote')
const table = require('text-table')
const color = require('ansicolors')
const styles = require('ansistyles')
const npa = require('npm-package-arg')
const pickManifest = require('npm-pick-manifest')

const Arborist = require('@npmcli/arborist')

const npm = require('./npm.js')
const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')
const ansiTrim = require('./utils/ansi-trim.js')

const usage = usageUtil('outdated',
  'npm outdated [[<@scope>/]<pkg> ...]'
)

function cmd (args, cb) {
  outdated(args)
    .then(() => cb())
    .catch(cb)
}

async function outdated (args) {
  const opts = npm.flatOptions
  const global = path.resolve(npm.globalDir, '..')
  const where = opts.global
    ? global
    : npm.prefix

  const arb = new Arborist({
    ...opts,
    path: where,
  })

  const tree = await arb.loadActual()
  const list = await outdated_(tree, args, opts)

  // sorts list alphabetically
  const outdated = list.sort((a, b) => a.name.localeCompare(b.name))

  // return if no outdated packages
  if (outdated.length === 0 && !opts.json)
    return

  // display results
  if (opts.json)
    output(makeJSON(outdated, opts))
  else if (opts.parseable)
    output(makeParseable(outdated, opts))
  else {
    const outList = outdated.map(x => makePretty(x, opts))
    const outHead = ['Package',
      'Current',
      'Wanted',
      'Latest',
      'Location',
      'Depended by',
    ]

    if (opts.long)
      outHead.push('Package Type', 'Homepage')
    const outTable = [outHead].concat(outList)

    if (opts.color)
      outTable[0] = outTable[0].map(heading => styles.underline(heading))

    const tableOpts = {
      align: ['l', 'r', 'r', 'r', 'l'],
      stringLength: s => ansiTrim(s).length,
    }
    output(table(outTable, tableOpts))
  }
}

async function outdated_ (tree, deps, opts) {
  const list = []

  const edges = new Set()
  function getEdges (nodes, type) {
    const getEdgesIn = (node) => {
      for (const edge of node.edgesIn)
        edges.add(edge)
    }

    const getEdgesOut = (node) => {
      if (opts.global) {
        for (const child of node.children.values())
          edges.add(child)
      } else {
        for (const edge of node.edgesOut.values())
          edges.add(edge)
      }
    }

    if (!nodes)
      return getEdgesOut(tree)
    for (const node of nodes) {
      type === 'edgesOut'
        ? getEdgesOut(node)
        : getEdgesIn(node)
    }
  }

  async function getPackument (spec) {
    const packument = await pacote.packument(spec, {
      ...npm.flatOptions,
      fullMetadata: npm.flatOptions.long,
      preferOnline: true,
    })
    return packument
  }

  async function getOutdatedInfo (edge) {
    const spec = npa(edge.name)
    const node = edge.to || edge
    const { path, location } = node
    const { version: current } = node.package || {}

    const type = edge.optional ? 'optionalDependencies'
      : edge.peer ? 'peerDependencies'
      : edge.dev ? 'devDependencies'
      : 'dependencies'

    for (const omitType of opts.omit || []) {
      if (node[omitType])
        return
    }

    // deps different from prod not currently
    // on disk are not included in the output
    if (edge.error === 'MISSING' && type !== 'dependencies')
      return

    try {
      const packument = await getPackument(spec)
      const expected = edge.spec
      // if it's not a range, version, or tag, skip it
      try {
        if (!npa(`${edge.name}@${edge.spec}`).registry)
          return null
      } catch (err) {
        return null
      }
      const wanted = pickManifest(packument, expected, npm.flatOptions)
      const latest = pickManifest(packument, '*', npm.flatOptions)

      if (
        !current ||
        current !== wanted.version ||
        wanted.version !== latest.version
      ) {
        list.push({
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
      // E404 errors, deps are just skipped {
      if (!(
        err.code === 'ETARGET' ||
        err.code === 'E403' ||
        err.code === 'E404')
      )
        throw err
    }
  }

  const p = []
  if (deps.length !== 0) {
    // specific deps
    for (let i = 0; i < deps.length; i++) {
      const nodes = tree.inventory.query('name', deps[i])
      getEdges(nodes, 'edgesIn')
    }
  } else {
    if (opts.all) {
      // all deps in tree
      const nodes = tree.inventory.values()
      getEdges(nodes, 'edgesOut')
    }
    // top-level deps
    getEdges()
  }

  for (const edge of edges)
    p.push(getOutdatedInfo(edge))

  await Promise.all(p)
  return list
}

// formatting functions
function makePretty (dep, opts) {
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

  if (opts.long) {
    columns[6] = type
    columns[7] = homepage
  }

  if (opts.color) {
    columns[0] = color[current === wanted ? 'yellow' : 'red'](columns[0]) // current
    columns[2] = color.green(columns[2]) // wanted
    columns[3] = color.magenta(columns[3]) // latest
  }

  return columns
}

// --parseable creates output like this:
// <fullpath>:<name@wanted>:<name@installed>:<name@latest>:<dependedby>
function makeParseable (list, opts) {
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
    if (opts.long)
      out.push(type, homepage)

    return out.join(':')
  }).join(os.EOL)
}

function makeJSON (list, opts) {
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
    if (opts.long) {
      out[name].type = type
      out[name].homepage = homepage
    }
  })
  return JSON.stringify(out, null, 2)
}

module.exports = Object.assign(cmd, { usage })
