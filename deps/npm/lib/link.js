const fs = require('fs')
const util = require('util')
const readdir = util.promisify(fs.readdir)
const { resolve } = require('path')

const Arborist = require('@npmcli/arborist')
const npa = require('npm-package-arg')
const rpj = require('read-package-json-fast')
const semver = require('semver')

const reifyFinish = require('./utils/reify-finish.js')

const ArboristWorkspaceCmd = require('./workspaces/arborist-cmd.js')
class Link extends ArboristWorkspaceCmd {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Symlink a package folder'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'link'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return [
      '(in package dir)',
      '[<@scope>/]<pkg>[@<version>]',
    ]
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'save',
      'save-exact',
      'global',
      'global-style',
      'legacy-bundling',
      'strict-peer-deps',
      'package-lock',
      'omit',
      'ignore-scripts',
      'audit',
      'bin-links',
      'fund',
      'dry-run',
      ...super.params,
    ]
  }

  async completion (opts) {
    const dir = this.npm.globalDir
    const files = await readdir(dir)
    return files.filter(f => !/^[._-]/.test(f))
  }

  exec (args, cb) {
    this.link(args).then(() => cb()).catch(cb)
  }

  async link (args) {
    if (this.npm.config.get('global')) {
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
    args = args.filter(a => resolve(a) !== this.npm.prefix)
    return args.length
      ? this.linkInstall(args)
      : this.linkPkg()
  }

  async linkInstall (args) {
    // load current packages from the global space,
    // and then add symlinks installs locally
    const globalTop = resolve(this.npm.globalDir, '..')
    const globalOpts = {
      ...this.npm.flatOptions,
      path: globalTop,
      log: this.npm.log,
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
    const missing = this.missingArgsFromTree(globals, args)
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
      Boolean(
        this.npm.config.find('save') !== 'default' ||
        this.npm.config.get('save-optional') ||
        this.npm.config.get('save-peer') ||
        this.npm.config.get('save-dev') ||
        this.npm.config.get('save-prod')
      )

    // create a new arborist instance for the local prefix and
    // reify all the pending names as symlinks there
    const localArb = new Arborist({
      ...this.npm.flatOptions,
      log: this.npm.log,
      path: this.npm.prefix,
      save,
    })
    await localArb.reify({
      ...this.npm.flatOptions,
      path: this.npm.prefix,
      log: this.npm.log,
      add: names.map(l => `file:${resolve(globalTop, 'node_modules', l)}`),
      save,
      workspaces: this.workspaces,
    })

    await reifyFinish(this.npm, localArb)
  }

  async linkPkg () {
    const wsp = this.workspacePaths
    const paths = wsp && wsp.length ? wsp : [this.npm.prefix]
    const add = paths.map(path => `file:${path}`)
    const globalTop = resolve(this.npm.globalDir, '..')
    const arb = new Arborist({
      ...this.npm.flatOptions,
      path: globalTop,
      log: this.npm.log,
      global: true,
    })
    await arb.reify({
      add,
      log: this.npm.log,
    })
    await reifyFinish(this.npm, arb)
  }

  // Returns a list of items that can't be fulfilled by
  // things found in the current arborist inventory
  missingArgsFromTree (tree, args) {
    if (tree.isLink)
      return this.missingArgsFromTree(tree.target, args)

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
}
module.exports = Link
