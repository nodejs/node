const fs = require('fs')
const util = require('util')
const readdir = util.promisify(fs.readdir)
const { resolve } = require('path')

const Arborist = require('@npmcli/arborist')
const npa = require('npm-package-arg')
const rpj = require('read-package-json-fast')
const semver = require('semver')

const npm = require('./npm.js')
const usageUtil = require('./utils/usage.js')
const reifyFinish = require('./utils/reify-finish.js')

const completion = async (opts) => {
  const dir = npm.globalDir
  const files = await readdir(dir)
  return files.filter(f => !/^[._-]/.test(f))
}

const usage = usageUtil(
  'link',
  'npm link (in package dir)' +
  '\nnpm link [<@scope>/]<pkg>[@<version>]'
)

const cmd = (args, cb) => link(args).then(() => cb()).catch(cb)

const link = async args => {
  if (npm.config.get('global')) {
    throw Object.assign(
      new Error(
        'link should never be --global.\n' +
        'Please re-run this command with --local'
      ),
      { code: 'ELINKGLOBAL' }
    )
  }

  // link with no args: symlink the folder to the global location
  // link with package arg: symlink the global to the local
  args = args.filter(a => resolve(a) !== npm.prefix)
  return args.length
    ? linkInstall(args)
    : linkPkg()
}

// Returns a list of items that can't be fulfilled by
// things found in the current arborist inventory
const missingArgsFromTree = (tree, args) => {
  if (tree.isLink)
    return missingArgsFromTree(tree.target, args)

  const foundNodes = []
  const missing = args.filter(a => {
    const arg = npa(a)
    const nodes = tree.children.values()
    const argFound = [...nodes].every(node => {
      // TODO: write tests for unmatching version specs, this is hard to test
      // atm but should be simple once we have a mocked registry again
      if (arg.name !== node.name /* istanbul ignore next */ || (
        arg.version &&
        !semver.satisfies(node.version, arg.version)
      )) {
        foundNodes.push(node)
        return true
      }
    })
    return argFound
  })

  // remote nodes from the loaded tree in order
  // to avoid dropping them later when reifying
  for (const node of foundNodes)
    node.parent = null

  return missing
}

const linkInstall = async args => {
  // load current packages from the global space,
  // and then add symlinks installs locally
  const globalTop = resolve(npm.globalDir, '..')
  const globalOpts = {
    ...npm.flatOptions,
    path: globalTop,
    global: true,
    prune: false,
  }
  const globalArb = new Arborist(globalOpts)

  // get only current top-level packages from the global space
  const globals = await globalArb.loadActual({
    filter: (node, kid) =>
      !node.isRoot || args.some(a => npa(a).name === kid),
  })

  // any extra arg that is missing from the current
  // global space should be reified there first
  const missing = missingArgsFromTree(globals, args)
  if (missing.length) {
    await globalArb.reify({
      ...globalOpts,
      add: missing,
    })
  }

  // get a list of module names that should be linked in the local prefix
  const names = []
  for (const a of args) {
    const arg = npa(a)
    names.push(
      arg.type === 'directory'
        ? (await rpj(resolve(arg.fetchSpec, 'package.json'))).name
        : arg.name
    )
  }

  // npm link should not save=true by default unless you're
  // using any of --save-dev or other types
  const save =
    Boolean(npm.config.find('save') !== 'default' || npm.flatOptions.saveType)

  // create a new arborist instance for the local prefix and
  // reify all the pending names as symlinks there
  const localArb = new Arborist({
    ...npm.flatOptions,
    path: npm.prefix,
    save,
  })
  await localArb.reify({
    ...npm.flatOptions,
    path: npm.prefix,
    add: names.map(l => `file:${resolve(globalTop, 'node_modules', l)}`),
    save,
  })

  await reifyFinish(localArb)
}

const linkPkg = async () => {
  const globalTop = resolve(npm.globalDir, '..')
  const arb = new Arborist({
    ...npm.flatOptions,
    path: globalTop,
    global: true,
  })
  await arb.reify({ add: [`file:${npm.prefix}`] })
  await reifyFinish(arb)
}

module.exports = Object.assign(cmd, { completion, usage })
