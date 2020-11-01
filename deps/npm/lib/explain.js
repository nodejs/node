const usageUtil = require('./utils/usage.js')
const npm = require('./npm.js')
const { explainNode } = require('./utils/explain-dep.js')
const completion = require('./utils/completion/installed-deep.js')
const output = require('./utils/output.js')
const Arborist = require('@npmcli/arborist')
const npa = require('npm-package-arg')
const semver = require('semver')
const { relative, resolve } = require('path')
const validName = require('validate-npm-package-name')

const usage = usageUtil('explain', 'npm explain <folder | specifier>')

const cmd = (args, cb) => explain(args).then(() => cb()).catch(cb)

const explain = async (args) => {
  if (!args.length)
    throw usage

  const arb = new Arborist({ path: npm.prefix, ...npm.flatOptions })
  const tree = await arb.loadActual()

  const nodes = new Set()
  for (const arg of args) {
    for (const node of getNodes(tree, arg))
      nodes.add(node)
  }
  if (nodes.size === 0)
    throw `No dependencies found matching ${args.join(', ')}`

  const expls = []
  for (const node of nodes) {
    const { extraneous, dev, optional, devOptional, peer } = node
    const expl = node.explain()
    if (extraneous)
      expl.extraneous = true
    else {
      expl.dev = dev
      expl.optional = optional
      expl.devOptional = devOptional
      expl.peer = peer
    }
    expls.push(expl)
  }

  if (npm.flatOptions.json)
    output(JSON.stringify(expls, null, 2))
  else {
    output(expls.map(expl => {
      return explainNode(expl, Infinity, npm.color)
    }).join('\n\n'))
  }
}

const getNodes = (tree, arg) => {
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
  const maybePath = relative(npm.prefix, resolve(maybeLoc))
    .replace(/\\/g, '/').replace(/\/+$/, '')
  const nodeByPath = tree.inventory.get(maybePath)
  if (nodeByPath)
    return [nodeByPath]

  // otherwise, try to select all matching nodes
  try {
    return getNodesByVersion(tree, arg)
  } catch (er) {
    return []
  }
}

const getNodesByVersion = (tree, arg) => {
  const spec = npa(arg, npm.prefix)
  if (spec.type !== 'version' && spec.type !== 'range')
    return []

  return tree.inventory.filter(node => {
    return node.package.name === spec.name &&
      semver.satisfies(node.package.version, spec.rawSpec)
  })
}

module.exports = Object.assign(cmd, { usage, completion })
