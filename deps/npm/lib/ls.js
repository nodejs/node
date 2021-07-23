const { resolve, relative, sep } = require('path')
const relativePrefix = `.${sep}`
const { EOL } = require('os')

const archy = require('archy')
const chalk = require('chalk')
const Arborist = require('@npmcli/arborist')
const { breadth } = require('treeverse')
const npa = require('npm-package-arg')

const completion = require('./utils/completion/installed-deep.js')

const _depth = Symbol('depth')
const _dedupe = Symbol('dedupe')
const _filteredBy = Symbol('filteredBy')
const _include = Symbol('include')
const _invalid = Symbol('invalid')
const _name = Symbol('name')
const _missing = Symbol('missing')
const _parent = Symbol('parent')
const _problems = Symbol('problems')
const _required = Symbol('required')
const _type = Symbol('type')
const ArboristWorkspaceCmd = require('./workspaces/arborist-cmd.js')

class LS extends ArboristWorkspaceCmd {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'List installed packages'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'ls'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[[<@scope>/]<pkg> ...]']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'all',
      'json',
      'long',
      'parseable',
      'global',
      'depth',
      'omit',
      'link',
      'package-lock-only',
      'unicode',
      ...super.params,
    ]
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  async completion (opts) {
    return completion(this.npm, opts)
  }

  exec (args, cb) {
    this.ls(args).then(() => cb()).catch(cb)
  }

  async ls (args) {
    const all = this.npm.config.get('all')
    const color = this.npm.color
    const depth = this.npm.config.get('depth')
    const dev = this.npm.config.get('dev')
    const development = this.npm.config.get('development')
    const global = this.npm.config.get('global')
    const json = this.npm.config.get('json')
    const link = this.npm.config.get('link')
    const long = this.npm.config.get('long')
    const only = this.npm.config.get('only')
    const parseable = this.npm.config.get('parseable')
    const prod = this.npm.config.get('prod')
    const production = this.npm.config.get('production')
    const unicode = this.npm.config.get('unicode')
    const packageLockOnly = this.npm.config.get('package-lock-only')

    const path = global ? resolve(this.npm.globalDir, '..') : this.npm.prefix

    const arb = new Arborist({
      global,
      ...this.npm.flatOptions,
      legacyPeerDeps: false,
      path,
    })
    const tree = await this.initTree({arb, args, packageLockOnly })

    // filters by workspaces nodes when using -w <workspace-name>
    // We only have to filter the first layer of edges, so we don't
    // explore anything that isn't part of the selected workspace set.
    let wsNodes
    if (this.workspaceNames && this.workspaceNames.length)
      wsNodes = arb.workspaceNodes(tree, this.workspaceNames)
    const filterBySelectedWorkspaces = edge => {
      if (!wsNodes || !wsNodes.length)
        return true

      if (edge.from.isProjectRoot) {
        return edge.to &&
          edge.to.isWorkspace &
          wsNodes.includes(edge.to.target)
      }

      return true
    }

    const seenItems = new Set()
    const seenNodes = new Map()
    const problems = new Set()

    // defines special handling of printed depth when filtering with args
    const filterDefaultDepth = depth === null ? Infinity : depth
    const depthToPrint = (all || args.length)
      ? filterDefaultDepth
      : (depth || 0)

    // add root node of tree to list of seenNodes
    seenNodes.set(tree.path, tree)

    // tree traversal happens here, using treeverse.breadth
    const result = await breadth({
      tree,
      // recursive method, `node` is going to be the current elem (starting from
      // the `tree` obj) that was just visited in the `visit` method below
      // `nodeResult` is going to be the returned `item` from `visit`
      getChildren (node, nodeResult) {
        const seenPaths = new Set()
        const workspace = node.isWorkspace
        const currentDepth = workspace ? 0 : node[_depth]
        const shouldSkipChildren =
          !(node instanceof Arborist.Node) || (currentDepth > depthToPrint)
        return (shouldSkipChildren)
          ? []
          : [...(node.target).edgesOut.values()]
            .filter(filterBySelectedWorkspaces)
            .filter(filterByEdgesTypes({
              currentDepth,
              dev,
              development,
              link,
              prod,
              production,
              only,
            }))
            .map(mapEdgesToNodes({ seenPaths }))
            .concat(appendExtraneousChildren({ node, seenPaths }))
            .sort(sortAlphabetically)
            .map(augmentNodesWithMetadata({
              args,
              currentDepth,
              nodeResult,
              seenNodes,
            }))
      },
      // visit each `node` of the `tree`, returning an `item` - these are
      // the elements that will be used to build the final output
      visit (node) {
        node[_problems] = getProblems(node, { global })

        const item = json
          ? getJsonOutputItem(node, { global, long })
          : parseable
            ? null
            : getHumanOutputItem(node, { args, color, global, long })

        // loop through list of node problems to add them to global list
        if (node[_include]) {
          for (const problem of node[_problems])
            problems.add(problem)
        }

        seenItems.add(item)

        // return a promise so we don't blow the stack
        return Promise.resolve(item)
      },
    })

    // handle the special case of a broken package.json in the root folder
    const [rootError] = tree.errors.filter(e =>
      e.code === 'EJSONPARSE' && e.path === resolve(path, 'package.json'))

    this.npm.output(
      json
        ? jsonOutput({ path, problems, result, rootError, seenItems })
        : parseable
          ? parseableOutput({ seenNodes, global, long })
          : humanOutput({ color, result, seenItems, unicode })
    )

    // if filtering items, should exit with error code on no results
    if (result && !result[_include] && args.length)
      process.exitCode = 1

    if (rootError) {
      throw Object.assign(
        new Error('Failed to parse root package.json'),
        { code: 'EJSONPARSE' }
      )
    }

    const shouldThrow = problems.size &&
      ![...problems].every(problem => problem.startsWith('extraneous:'))

    if (shouldThrow) {
      throw Object.assign(
        new Error([...problems].join(EOL)),
        { code: 'ELSPROBLEMS' }
      )
    }
  }

  async initTree ({ arb, args, packageLockOnly }) {
    const tree = await (
      packageLockOnly
        ? arb.loadVirtual()
        : arb.loadActual()
    )

    tree[_include] = args.length === 0
    tree[_depth] = 0

    return tree
  }
}
module.exports = LS

const isGitNode = (node) => {
  if (!node.resolved)
    return

  try {
    const { type } = npa(node.resolved)
    return type === 'git' || type === 'hosted'
  } catch (err) {
    return false
  }
}

const isOptional = (node) =>
  node[_type] === 'optional' || node[_type] === 'peerOptional'

const isExtraneous = (node, { global }) =>
  node.extraneous && !global

const getProblems = (node, { global }) => {
  const problems = new Set()

  if (node[_missing] && !isOptional(node))
    problems.add(`missing: ${node.pkgid}, required by ${node[_missing]}`)

  if (node[_invalid])
    problems.add(`invalid: ${node.pkgid} ${node.path}`)

  if (isExtraneous(node, { global }))
    problems.add(`extraneous: ${node.pkgid} ${node.path}`)

  return problems
}

// annotates _parent and _include metadata into the resulting
// item obj allowing for filtering out results during output
const augmentItemWithIncludeMetadata = (node, item) => {
  item[_parent] = node[_parent]
  item[_include] = node[_include]

  // append current item to its parent.nodes which is the
  // structure expected by archy in order to print tree
  if (node[_include]) {
    // includes all ancestors of included node
    let p = node[_parent]
    while (p) {
      p[_include] = true
      p = p[_parent]
    }
  }

  return item
}

const getHumanOutputItem = (node, { args, color, global, long }) => {
  const { pkgid, path } = node
  const workspacePkgId = color ? chalk.green(pkgid) : pkgid
  let printable = node.isWorkspace ? workspacePkgId : pkgid

  // special formatting for top-level package name
  if (node.isRoot) {
    const hasNoPackageJson = !Object.keys(node.package).length
    if (hasNoPackageJson || global)
      printable = path
    else
      printable += `${long ? EOL : ' '}${path}`
  }

  const highlightDepName =
    color && args.length && node[_filteredBy]
  const missingColor = isOptional(node)
    ? chalk.yellow.bgBlack
    : chalk.red.bgBlack
  const missingMsg = `UNMET ${isOptional(node) ? 'OPTIONAL ' : ''}DEPENDENCY`
  const targetLocation = node.root
    ? relative(node.root.realpath, node.realpath)
    : node.targetLocation
  const invalid = node[_invalid]
    ? `invalid: ${node[_invalid]}`
    : ''
  const label =
    (
      node[_missing]
        ? (color ? missingColor(missingMsg) : missingMsg) + ' '
        : ''
    ) +
    `${highlightDepName ? chalk.yellow.bgBlack(printable) : printable}` +
    (
      node[_dedupe]
        ? ' ' + (color ? chalk.gray('deduped') : 'deduped')
        : ''
    ) +
    (
      invalid
        ? ' ' + (color ? chalk.red.bgBlack(invalid) : invalid)
        : ''
    ) +
    (
      isExtraneous(node, { global })
        ? ' ' + (color ? chalk.green.bgBlack('extraneous') : 'extraneous')
        : ''
    ) +
    (isGitNode(node) ? ` (${node.resolved})` : '') +
    (node.isLink ? ` -> ${relativePrefix}${targetLocation}` : '') +
    (long ? `${EOL}${node.package.description || ''}` : '')

  return augmentItemWithIncludeMetadata(node, { label, nodes: [] })
}

const getJsonOutputItem = (node, { global, long }) => {
  const item = {}

  if (node.version)
    item.version = node.version

  if (node.resolved)
    item.resolved = node.resolved

  item[_name] = node.name

  // special formatting for top-level package name
  const hasPackageJson =
    node && node.package && Object.keys(node.package).length
  if (node.isRoot && hasPackageJson)
    item.name = node.package.name || node.name

  if (long && !node[_missing]) {
    item.name = item[_name]
    const { dependencies, ...packageInfo } = node.package
    Object.assign(item, packageInfo)
    item.extraneous = false
    item.path = node.path
    item._dependencies = {
      ...node.package.dependencies,
      ...node.package.optionalDependencies,
    }
    item.devDependencies = node.package.devDependencies || {}
    item.peerDependencies = node.package.peerDependencies || {}
  }

  // augment json output items with extra metadata
  if (isExtraneous(node, { global }))
    item.extraneous = true

  if (node[_invalid])
    item.invalid = node[_invalid]

  if (node[_missing] && !isOptional(node)) {
    item.required = node[_required]
    item.missing = true
  }
  if (node[_include] && node[_problems] && node[_problems].size)
    item.problems = [...node[_problems]]

  return augmentItemWithIncludeMetadata(node, item)
}

const filterByEdgesTypes = ({
  currentDepth,
  dev,
  development,
  link,
  prod,
  production,
  only,
}) => {
  // filter deps by type, allows for: `npm ls --dev`, `npm ls --prod`,
  // `npm ls --link`, `npm ls --only=dev`, etc
  const filterDev = currentDepth === 0 &&
    (dev || development || /^dev(elopment)?$/.test(only))
  const filterProd = currentDepth === 0 &&
    (prod || production || /^prod(uction)?$/.test(only))
  const filterLink = currentDepth === 0 && link

  return (edge) =>
    (filterDev ? edge.dev : true) &&
    (filterProd ? (!edge.dev && !edge.peer && !edge.peerOptional) : true) &&
    (filterLink ? (edge.to && edge.to.isLink) : true)
}

const appendExtraneousChildren = ({ node, seenPaths }) =>
  // extraneous children are not represented
  // in edges out, so here we add them to the list:
  [...node.children.values()]
    .filter(i => !seenPaths.has(i.path) && i.extraneous)

const mapEdgesToNodes = ({ seenPaths }) => (edge) => {
  let node = edge.to

  // if the edge is linking to a missing node, we go ahead
  // and create a new obj that will represent the missing node
  if (edge.missing || (edge.optional && !node)) {
    const { name, spec } = edge
    const pkgid = `${name}@${spec}`
    node = { name, pkgid, [_missing]: edge.from.pkgid }
  }

  // keeps track of a set of seen paths to avoid the edge case in which a tree
  // item would appear twice given that it's a children of an extraneous item,
  // so it's marked extraneous but it will ALSO show up in edgesOuts of
  // its parent so it ends up as two diff nodes if we don't track it
  if (node.path)
    seenPaths.add(node.path)

  node[_required] = edge.spec || '*'
  node[_type] = edge.type

  if (edge.invalid) {
    const spec = JSON.stringify(node[_required])
    const from = edge.from.location || 'the root project'
    node[_invalid] = (node[_invalid] ? node[_invalid] + ', ' : '') +
      (`${spec} from ${from}`)
  }

  return node
}

const filterByPositionalArgs = (args, { node }) =>
  args.length > 0 ? args.some(
    (spec) => (node.satisfies && node.satisfies(spec))
  ) : true

const augmentNodesWithMetadata = ({
  args,
  currentDepth,
  nodeResult,
  seenNodes,
}) => (node) => {
  // if the original edge was a deduped dep, treeverse will fail to
  // revisit that node in tree traversal logic, so we make it so that
  // we have a diff obj for deduped nodes:
  if (seenNodes.has(node.path)) {
    const { realpath, root } = node
    const targetLocation = root ? relative(root.realpath, realpath)
      : node.targetLocation
    node = {
      name: node.name,
      version: node.version,
      pkgid: node.pkgid,
      package: node.package,
      path: node.path,
      isLink: node.isLink,
      realpath: node.realpath,
      targetLocation,
      [_type]: node[_type],
      [_invalid]: node[_invalid],
      [_missing]: node[_missing],
      // if it's missing, it's not deduped, it's just missing
      [_dedupe]: !node[_missing],
    }
  } else {
    // keeps track of already seen nodes in order to check for dedupes
    seenNodes.set(node.path, node)
  }

  // _parent is going to be a ref to a treeverse-visited node (returned from
  // getHumanOutputItem, getJsonOutputItem, etc) so that we have an easy
  // shortcut to place new nodes in their right place during tree traversal
  node[_parent] = nodeResult
  // _include is the property that allow us to filter based on position args
  // e.g: `npm ls foo`, `npm ls simple-output@2`
  // _filteredBy is used to apply extra color info to the item that
  // was used in args in order to filter
  node[_filteredBy] = node[_include] =
    filterByPositionalArgs(args, { node: seenNodes.get(node.path) })
  // _depth keeps track of how many levels deep tree traversal currently is
  // so that we can `npm ls --depth=1`
  node[_depth] = currentDepth + 1

  return node
}

const sortAlphabetically = (a, b) =>
  a.pkgid.localeCompare(b.pkgid, 'en')

const humanOutput = ({ color, result, seenItems, unicode }) => {
  // we need to traverse the entire tree in order to determine which items
  // should be included (since a nested transitive included dep will make it
  // so that all its ancestors should be displayed)
  // here is where we put items in their expected place for archy output
  for (const item of seenItems) {
    if (item[_include] && item[_parent])
      item[_parent].nodes.push(item)
  }

  if (!result.nodes.length)
    result.nodes = ['(empty)']

  const archyOutput = archy(result, '', { unicode })
  return color ? chalk.reset(archyOutput) : archyOutput
}

const jsonOutput = ({ path, problems, result, rootError, seenItems }) => {
  if (problems.size)
    result.problems = [...problems]

  if (rootError) {
    result.problems = [
      ...(result.problems || []),
      ...[`error in ${path}: Failed to parse root package.json`],
    ]
    result.invalid = true
  }

  // we need to traverse the entire tree in order to determine which items
  // should be included (since a nested transitive included dep will make it
  // so that all its ancestors should be displayed)
  // here is where we put items in their expected place for json output
  for (const item of seenItems) {
    // append current item to its parent item.dependencies obj in order
    // to provide a json object structure that represents the installed tree
    if (item[_include] && item[_parent]) {
      if (!item[_parent].dependencies)
        item[_parent].dependencies = {}

      item[_parent].dependencies[item[_name]] = item
    }
  }

  return JSON.stringify(result, null, 2)
}

const parseableOutput = ({ global, long, seenNodes }) => {
  let out = ''
  for (const node of seenNodes.values()) {
    if (node.path && node[_include]) {
      out += node.path
      if (long) {
        out += `:${node.pkgid}`
        out += node.path !== node.realpath ? `:${node.realpath}` : ''
        out += isExtraneous(node, { global }) ? ':EXTRANEOUS' : ''
        out += node[_invalid] ? ':INVALID' : ''
      }
      out += EOL
    }
  }
  return out.trim()
}
