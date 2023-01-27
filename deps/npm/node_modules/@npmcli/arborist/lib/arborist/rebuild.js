// Arborist.rebuild({path = this.path}) will do all the binlinks and
// bundle building needed.  Called by reify, and by `npm rebuild`.

const localeCompare = require('@isaacs/string-locale-compare')('en')
const { depth: dfwalk } = require('treeverse')
const promiseAllRejectLate = require('promise-all-reject-late')
const rpj = require('read-package-json-fast')
const binLinks = require('bin-links')
const runScript = require('@npmcli/run-script')
const promiseCallLimit = require('promise-call-limit')
const { resolve } = require('path')
const {
  isNodeGypPackage,
  defaultGypInstallScript,
} = require('@npmcli/node-gyp')
const log = require('proc-log')

const boolEnv = b => b ? '1' : ''
const sortNodes = (a, b) =>
  (a.depth - b.depth) || localeCompare(a.path, b.path)

const _workspaces = Symbol.for('workspaces')
const _build = Symbol('build')
const _loadDefaultNodes = Symbol('loadDefaultNodes')
const _retrieveNodesByType = Symbol('retrieveNodesByType')
const _resetQueues = Symbol('resetQueues')
const _rebuildBundle = Symbol('rebuildBundle')
const _ignoreScripts = Symbol('ignoreScripts')
const _binLinks = Symbol('binLinks')
const _oldMeta = Symbol('oldMeta')
const _createBinLinks = Symbol('createBinLinks')
const _doHandleOptionalFailure = Symbol('doHandleOptionalFailure')
const _linkAllBins = Symbol('linkAllBins')
const _runScripts = Symbol('runScripts')
const _buildQueues = Symbol('buildQueues')
const _addToBuildSet = Symbol('addToBuildSet')
const _checkBins = Symbol.for('checkBins')
const _queues = Symbol('queues')
const _scriptShell = Symbol('scriptShell')
const _includeWorkspaceRoot = Symbol.for('includeWorkspaceRoot')
const _workspacesEnabled = Symbol.for('workspacesEnabled')

const _force = Symbol.for('force')
const _global = Symbol.for('global')

// defined by reify mixin
const _handleOptionalFailure = Symbol.for('handleOptionalFailure')
const _trashList = Symbol.for('trashList')

module.exports = cls => class Builder extends cls {
  constructor (options) {
    super(options)

    const {
      ignoreScripts = false,
      scriptShell,
      binLinks = true,
      rebuildBundle = true,
    } = options

    this.scriptsRun = new Set()
    this[_binLinks] = binLinks
    this[_ignoreScripts] = !!ignoreScripts
    this[_scriptShell] = scriptShell
    this[_rebuildBundle] = !!rebuildBundle
    this[_resetQueues]()
    this[_oldMeta] = null
  }

  async rebuild ({ nodes, handleOptionalFailure = false } = {}) {
    // nothing to do if we're not building anything!
    if (this[_ignoreScripts] && !this[_binLinks]) {
      return
    }

    // when building for the first time, as part of reify, we ignore
    // failures in optional nodes, and just delete them.  however, when
    // running JUST a rebuild, we treat optional failures as real fails
    this[_doHandleOptionalFailure] = handleOptionalFailure

    if (!nodes) {
      nodes = await this[_loadDefaultNodes]()
    }

    // separates links nodes so that it can run
    // prepare scripts and link bins in the expected order
    process.emit('time', 'build')

    const {
      depNodes,
      linkNodes,
    } = this[_retrieveNodesByType](nodes)

    // build regular deps
    await this[_build](depNodes, {})

    // build link deps
    if (linkNodes.size) {
      this[_resetQueues]()
      await this[_build](linkNodes, { type: 'links' })
    }

    process.emit('timeEnd', 'build')
  }

  // if we don't have a set of nodes, then just rebuild
  // the actual tree on disk.
  async [_loadDefaultNodes] () {
    let nodes
    const tree = await this.loadActual()
    let filterSet
    if (!this[_workspacesEnabled]) {
      filterSet = this.excludeWorkspacesDependencySet(tree)
      nodes = tree.inventory.filter(node =>
        filterSet.has(node) || node.isProjectRoot
      )
    } else if (this[_workspaces] && this[_workspaces].length) {
      filterSet = this.workspaceDependencySet(
        tree,
        this[_workspaces],
        this[_includeWorkspaceRoot]
      )
      nodes = tree.inventory.filter(node => filterSet.has(node))
    } else {
      nodes = tree.inventory.values()
    }
    return nodes
  }

  [_retrieveNodesByType] (nodes) {
    const depNodes = new Set()
    const linkNodes = new Set()

    for (const node of nodes) {
      if (node.isLink) {
        linkNodes.add(node)
      } else {
        depNodes.add(node)
      }
    }

    // deduplicates link nodes and their targets, avoids
    // calling lifecycle scripts twice when running `npm rebuild`
    // ref: https://github.com/npm/cli/issues/2905
    //
    // we avoid doing so if global=true since `bin-links` relies
    // on having the target nodes available in global mode.
    if (!this[_global]) {
      for (const node of linkNodes) {
        depNodes.delete(node.target)
      }
    }

    return {
      depNodes,
      linkNodes,
    }
  }

  [_resetQueues] () {
    this[_queues] = {
      preinstall: [],
      install: [],
      postinstall: [],
      prepare: [],
      bin: [],
    }
  }

  async [_build] (nodes, { type = 'deps' }) {
    process.emit('time', `build:${type}`)

    await this[_buildQueues](nodes)

    if (!this[_ignoreScripts]) {
      await this[_runScripts]('preinstall')
    }

    // links should run prepare scripts and only link bins after that
    if (type === 'links') {
      await this[_runScripts]('prepare')
    }
    if (this[_binLinks]) {
      await this[_linkAllBins]()
    }

    if (!this[_ignoreScripts]) {
      await this[_runScripts]('install')
      await this[_runScripts]('postinstall')
    }

    process.emit('timeEnd', `build:${type}`)
  }

  async [_buildQueues] (nodes) {
    process.emit('time', 'build:queue')
    const set = new Set()

    const promises = []
    for (const node of nodes) {
      promises.push(this[_addToBuildSet](node, set))

      // if it has bundle deps, add those too, if rebuildBundle
      if (this[_rebuildBundle] !== false) {
        const bd = node.package.bundleDependencies
        if (bd && bd.length) {
          dfwalk({
            tree: node,
            leave: node => promises.push(this[_addToBuildSet](node, set)),
            getChildren: node => [...node.children.values()],
            filter: node => node.inBundle,
          })
        }
      }
    }
    await promiseAllRejectLate(promises)

    // now sort into the queues for the 4 things we have to do
    // run in the same predictable order that buildIdealTree uses
    // there's no particular reason for doing it in this order rather
    // than another, but sorting *somehow* makes it consistent.
    const queue = [...set].sort(sortNodes)

    for (const node of queue) {
      const { package: { bin, scripts = {} } } = node.target
      const { preinstall, install, postinstall, prepare } = scripts
      const tests = { bin, preinstall, install, postinstall, prepare }
      for (const [key, has] of Object.entries(tests)) {
        if (has) {
          this[_queues][key].push(node)
        }
      }
    }
    process.emit('timeEnd', 'build:queue')
  }

  async [_checkBins] (node) {
    // if the node is a global top, and we're not in force mode, then
    // any existing bins need to either be missing, or a symlink into
    // the node path.  Otherwise a package can have a preinstall script
    // that unlinks something, to allow them to silently overwrite system
    // binaries, which is unsafe and insecure.
    if (!node.globalTop || this[_force]) {
      return
    }
    const { path, package: pkg } = node
    await binLinks.checkBins({ pkg, path, top: true, global: true })
  }

  async [_addToBuildSet] (node, set, refreshed = false) {
    if (set.has(node)) {
      return
    }

    if (this[_oldMeta] === null) {
      const { root: { meta } } = node
      this[_oldMeta] = meta && meta.loadedFromDisk &&
        !(meta.originalLockfileVersion >= 2)
    }

    const { package: pkg, hasInstallScript } = node.target
    const { gypfile, bin, scripts = {} } = pkg

    const { preinstall, install, postinstall, prepare } = scripts
    const anyScript = preinstall || install || postinstall || prepare
    if (!refreshed && !anyScript && (hasInstallScript || this[_oldMeta])) {
      // we either have an old metadata (and thus might have scripts)
      // or we have an indication that there's install scripts (but
      // don't yet know what they are) so we have to load the package.json
      // from disk to see what the deal is.  Failure here just means
      // no scripts to add, probably borked package.json.
      // add to the set then remove while we're reading the pj, so we
      // don't accidentally hit it multiple times.
      set.add(node)
      const pkg = await rpj(node.path + '/package.json').catch(() => ({}))
      set.delete(node)

      const { scripts = {} } = pkg
      node.package.scripts = scripts
      return this[_addToBuildSet](node, set, true)
    }

    // Rebuild node-gyp dependencies lacking an install or preinstall script
    // note that 'scripts' might be missing entirely, and the package may
    // set gypfile:false to avoid this automatic detection.
    const isGyp = gypfile !== false &&
      !install &&
      !preinstall &&
      await isNodeGypPackage(node.path)

    if (bin || preinstall || install || postinstall || prepare || isGyp) {
      if (bin) {
        await this[_checkBins](node)
      }
      if (isGyp) {
        scripts.install = defaultGypInstallScript
        node.package.scripts = scripts
      }
      set.add(node)
    }
  }

  async [_runScripts] (event) {
    const queue = this[_queues][event]

    if (!queue.length) {
      return
    }

    process.emit('time', `build:run:${event}`)
    const stdio = this.options.foregroundScripts ? 'inherit' : 'pipe'
    const limit = this.options.foregroundScripts ? 1 : undefined
    await promiseCallLimit(queue.map(node => async () => {
      const {
        path,
        integrity,
        resolved,
        optional,
        peer,
        dev,
        devOptional,
        package: pkg,
        location,
      } = node.target

      // skip any that we know we'll be deleting
      if (this[_trashList].has(path)) {
        return
      }

      const timer = `build:run:${event}:${location}`
      process.emit('time', timer)
      log.info('run', pkg._id, event, location, pkg.scripts[event])
      const env = {
        npm_package_resolved: resolved,
        npm_package_integrity: integrity,
        npm_package_json: resolve(path, 'package.json'),
        npm_package_optional: boolEnv(optional),
        npm_package_dev: boolEnv(dev),
        npm_package_peer: boolEnv(peer),
        npm_package_dev_optional:
          boolEnv(devOptional && !dev && !optional),
      }
      const runOpts = {
        event,
        path,
        pkg,
        stdio,
        env,
        scriptShell: this[_scriptShell],
      }
      const p = runScript(runOpts).catch(er => {
        const { code, signal } = er
        log.info('run', pkg._id, event, { code, signal })
        throw er
      }).then(({ args, code, signal, stdout, stderr }) => {
        this.scriptsRun.add({
          pkg,
          path,
          event,
          // I do not know why this needs to be on THIS line but refactoring
          // this function would be quite a process
          // eslint-disable-next-line promise/always-return
          cmd: args && args[args.length - 1],
          env,
          code,
          signal,
          stdout,
          stderr,
        })
        log.info('run', pkg._id, event, { code, signal })
      })

      await (this[_doHandleOptionalFailure]
        ? this[_handleOptionalFailure](node, p)
        : p)

      process.emit('timeEnd', timer)
    }), limit)
    process.emit('timeEnd', `build:run:${event}`)
  }

  async [_linkAllBins] () {
    const queue = this[_queues].bin
    if (!queue.length) {
      return
    }

    process.emit('time', 'build:link')
    const promises = []
    // sort the queue by node path, so that the module-local collision
    // detector in bin-links will always resolve the same way.
    for (const node of queue.sort(sortNodes)) {
      promises.push(this[_createBinLinks](node))
    }

    await promiseAllRejectLate(promises)
    process.emit('timeEnd', 'build:link')
  }

  async [_createBinLinks] (node) {
    if (this[_trashList].has(node.path)) {
      return
    }

    process.emit('time', `build:link:${node.location}`)

    const p = binLinks({
      pkg: node.package,
      path: node.path,
      top: !!(node.isTop || node.globalTop),
      force: this[_force],
      global: !!node.globalTop,
    })

    await (this[_doHandleOptionalFailure]
      ? this[_handleOptionalFailure](node, p)
      : p)

    process.emit('timeEnd', `build:link:${node.location}`)
  }
}
