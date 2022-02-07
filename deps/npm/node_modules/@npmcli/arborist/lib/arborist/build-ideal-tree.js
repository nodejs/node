// mixin implementing the buildIdealTree method
const localeCompare = require('@isaacs/string-locale-compare')('en')
const rpj = require('read-package-json-fast')
const npa = require('npm-package-arg')
const pacote = require('pacote')
const cacache = require('cacache')
const promiseCallLimit = require('promise-call-limit')
const realpath = require('../../lib/realpath.js')
const { resolve, dirname } = require('path')
const { promisify } = require('util')
const treeCheck = require('../tree-check.js')
const readdir = promisify(require('readdir-scoped-modules'))
const fs = require('fs')
const lstat = promisify(fs.lstat)
const readlink = promisify(fs.readlink)
const { depth } = require('treeverse')

const {
  OK,
  REPLACE,
  CONFLICT,
} = require('../can-place-dep.js')
const PlaceDep = require('../place-dep.js')

const debug = require('../debug.js')
const fromPath = require('../from-path.js')
const calcDepFlags = require('../calc-dep-flags.js')
const Shrinkwrap = require('../shrinkwrap.js')
const { defaultLockfileVersion } = Shrinkwrap
const Node = require('../node.js')
const Link = require('../link.js')
const addRmPkgDeps = require('../add-rm-pkg-deps.js')
const optionalSet = require('../optional-set.js')
const { checkEngine, checkPlatform } = require('npm-install-checks')

const relpath = require('../relpath.js')

// note: some of these symbols are shared so we can hit
// them with unit tests and reuse them across mixins
const _complete = Symbol('complete')
const _depsSeen = Symbol('depsSeen')
const _depsQueue = Symbol('depsQueue')
const _currentDep = Symbol('currentDep')
const _updateAll = Symbol.for('updateAll')
const _mutateTree = Symbol('mutateTree')
const _flagsSuspect = Symbol.for('flagsSuspect')
const _workspaces = Symbol.for('workspaces')
const _prune = Symbol('prune')
const _preferDedupe = Symbol('preferDedupe')
const _legacyBundling = Symbol('legacyBundling')
const _parseSettings = Symbol('parseSettings')
const _initTree = Symbol('initTree')
const _applyUserRequests = Symbol('applyUserRequests')
const _applyUserRequestsToNode = Symbol('applyUserRequestsToNode')
const _inflateAncientLockfile = Symbol('inflateAncientLockfile')
const _buildDeps = Symbol('buildDeps')
const _buildDepStep = Symbol('buildDepStep')
const _nodeFromEdge = Symbol('nodeFromEdge')
const _nodeFromSpec = Symbol('nodeFromSpec')
const _fetchManifest = Symbol('fetchManifest')
const _problemEdges = Symbol('problemEdges')
const _manifests = Symbol('manifests')
const _loadWorkspaces = Symbol.for('loadWorkspaces')
const _linkFromSpec = Symbol('linkFromSpec')
const _loadPeerSet = Symbol('loadPeerSet')
const _updateNames = Symbol.for('updateNames')
const _fixDepFlags = Symbol('fixDepFlags')
const _resolveLinks = Symbol('resolveLinks')
const _rootNodeFromPackage = Symbol('rootNodeFromPackage')
const _add = Symbol('add')
const _resolvedAdd = Symbol.for('resolvedAdd')
const _queueNamedUpdates = Symbol('queueNamedUpdates')
const _queueVulnDependents = Symbol('queueVulnDependents')
const _avoidRange = Symbol('avoidRange')
const _shouldUpdateNode = Symbol('shouldUpdateNode')
const resetDepFlags = require('../reset-dep-flags.js')
const _loadFailures = Symbol('loadFailures')
const _pruneFailedOptional = Symbol('pruneFailedOptional')
const _linkNodes = Symbol('linkNodes')
const _follow = Symbol('follow')
const _globalStyle = Symbol('globalStyle')
const _globalRootNode = Symbol('globalRootNode')
const _isVulnerable = Symbol.for('isVulnerable')
const _usePackageLock = Symbol.for('usePackageLock')
const _rpcache = Symbol.for('realpathCache')
const _stcache = Symbol.for('statCache')
const _updateFilePath = Symbol('updateFilePath')
const _followSymlinkPath = Symbol('followSymlinkPath')
const _getRelpathSpec = Symbol('getRelpathSpec')
const _retrieveSpecName = Symbol('retrieveSpecName')
const _strictPeerDeps = Symbol('strictPeerDeps')
const _checkEngineAndPlatform = Symbol('checkEngineAndPlatform')
const _checkEngine = Symbol('checkEngine')
const _checkPlatform = Symbol('checkPlatform')
const _virtualRoots = Symbol('virtualRoots')
const _virtualRoot = Symbol('virtualRoot')
const _includeWorkspaceRoot = Symbol.for('includeWorkspaceRoot')

const _failPeerConflict = Symbol('failPeerConflict')
const _explainPeerConflict = Symbol('explainPeerConflict')
const _edgesOverridden = Symbol('edgesOverridden')
// exposed symbol for unit testing the placeDep method directly
const _peerSetSource = Symbol.for('peerSetSource')

// used by Reify mixin
const _force = Symbol.for('force')
const _explicitRequests = Symbol('explicitRequests')
const _global = Symbol.for('global')
const _idealTreePrune = Symbol.for('idealTreePrune')

module.exports = cls => class IdealTreeBuilder extends cls {
  constructor (options) {
    super(options)

    // normalize trailing slash
    const registry = options.registry || 'https://registry.npmjs.org'
    options.registry = this.registry = registry.replace(/\/+$/, '') + '/'

    const {
      follow = false,
      force = false,
      global = false,
      globalStyle = false,
      idealTree = null,
      includeWorkspaceRoot = false,
      legacyPeerDeps = false,
      packageLock = true,
      strictPeerDeps = false,
      workspaces = [],
    } = options

    this[_workspaces] = workspaces || []
    this[_force] = !!force
    this[_strictPeerDeps] = !!strictPeerDeps

    this.idealTree = idealTree
    this.legacyPeerDeps = legacyPeerDeps

    this[_usePackageLock] = packageLock
    this[_global] = !!global
    this[_globalStyle] = this[_global] || globalStyle
    this[_follow] = !!follow

    if (this[_workspaces].length && this[_global]) {
      throw new Error('Cannot operate on workspaces in global mode')
    }

    this[_explicitRequests] = new Set()
    this[_preferDedupe] = false
    this[_legacyBundling] = false
    this[_depsSeen] = new Set()
    this[_depsQueue] = []
    this[_currentDep] = null
    this[_updateNames] = []
    this[_updateAll] = false
    this[_mutateTree] = false
    this[_loadFailures] = new Set()
    this[_linkNodes] = new Set()
    this[_manifests] = new Map()
    this[_edgesOverridden] = new Set()
    this[_resolvedAdd] = []

    // a map of each module in a peer set to the thing that depended on
    // that set of peers in the first place.  Use a WeakMap so that we
    // don't hold onto references for nodes that are garbage collected.
    this[_peerSetSource] = new WeakMap()
    this[_virtualRoots] = new Map()

    this[_includeWorkspaceRoot] = includeWorkspaceRoot
  }

  get explicitRequests () {
    return new Set(this[_explicitRequests])
  }

  // public method
  async buildIdealTree (options = {}) {
    if (this.idealTree) {
      return this.idealTree
    }

    // allow the user to set reify options on the ctor as well.
    // XXX: deprecate separate reify() options object.
    options = { ...this.options, ...options }

    // an empty array or any falsey value is the same as null
    if (!options.add || options.add.length === 0) {
      options.add = null
    }
    if (!options.rm || options.rm.length === 0) {
      options.rm = null
    }

    process.emit('time', 'idealTree')

    if (!options.add && !options.rm && !options.update && this[_global]) {
      throw new Error('global requires add, rm, or update option')
    }

    // first get the virtual tree, if possible.  If there's a lockfile, then
    // that defines the ideal tree, unless the root package.json is not
    // satisfied by what the ideal tree provides.
    // from there, we start adding nodes to it to satisfy the deps requested
    // by the package.json in the root.

    this[_parseSettings](options)

    // start tracker block
    this.addTracker('idealTree')

    try {
      await this[_initTree]()
      await this[_inflateAncientLockfile]()
      await this[_applyUserRequests](options)
      await this[_buildDeps]()
      await this[_fixDepFlags]()
      await this[_pruneFailedOptional]()
      await this[_checkEngineAndPlatform]()
    } finally {
      process.emit('timeEnd', 'idealTree')
      this.finishTracker('idealTree')
    }

    return treeCheck(this.idealTree)
  }

  async [_checkEngineAndPlatform] () {
    for (const node of this.idealTree.inventory.values()) {
      if (!node.optional) {
        this[_checkEngine](node)
        this[_checkPlatform](node)
      }
    }
  }

  [_checkPlatform] (node) {
    checkPlatform(node.package, this[_force])
  }

  [_checkEngine] (node) {
    const { engineStrict, npmVersion, nodeVersion } = this.options
    const c = () =>
      checkEngine(node.package, npmVersion, nodeVersion, this[_force])

    if (engineStrict) {
      c()
    } else {
      try {
        c()
      } catch (er) {
        this.log.warn(er.code, er.message, {
          package: er.pkgid,
          required: er.required,
          current: er.current,
        })
      }
    }
  }

  [_parseSettings] (options) {
    const update = options.update === true ? { all: true }
      : Array.isArray(options.update) ? { names: options.update }
      : options.update || {}

    if (update.all || !Array.isArray(update.names)) {
      update.names = []
    }

    this[_complete] = !!options.complete
    this[_preferDedupe] = !!options.preferDedupe
    this[_legacyBundling] = !!options.legacyBundling

    // validates list of update names, they must
    // be dep names only, no semver ranges are supported
    for (const name of update.names) {
      const spec = npa(name)
      const validationError =
        new TypeError(`Update arguments must not contain package version specifiers

Try using the package name instead, e.g:
    npm update ${spec.name}`)
      validationError.code = 'EUPDATEARGS'

      if (spec.fetchSpec !== 'latest') {
        throw validationError
      }
    }
    this[_updateNames] = update.names

    this[_updateAll] = update.all
    // we prune by default unless explicitly set to boolean false
    this[_prune] = options.prune !== false

    // set if we add anything, but also set here if we know we'll make
    // changes and thus have to maybe prune later.
    this[_mutateTree] = !!(
      options.add ||
      options.rm ||
      update.all ||
      update.names.length
    )
  }

  // load the initial tree, either the virtualTree from a shrinkwrap,
  // or just the root node from a package.json
  [_initTree] () {
    process.emit('time', 'idealTree:init')
    return (
      this[_global] ? this[_globalRootNode]()
      : rpj(this.path + '/package.json').then(
        pkg => this[_rootNodeFromPackage](pkg),
        er => {
          if (er.code === 'EJSONPARSE') {
            throw er
          }
          return this[_rootNodeFromPackage]({})
        }
      ))
      .then(root => this[_loadWorkspaces](root))
      // ok to not have a virtual tree.  probably initial install.
      // When updating all, we load the shrinkwrap, but don't bother
      // to build out the full virtual tree from it, since we'll be
      // reconstructing it anyway.
      .then(root => this[_global] ? root
      : !this[_usePackageLock] || this[_updateAll]
        ? Shrinkwrap.reset({
          path: this.path,
          lockfileVersion: this.options.lockfileVersion,
        }).then(meta => Object.assign(root, { meta }))
        : this.loadVirtual({ root }))

      // if we don't have a lockfile to go from, then start with the
      // actual tree, so we only make the minimum required changes.
      // don't do this for global installs or updates, because in those
      // cases we don't use a lockfile anyway.
      // Load on a new Arborist object, so the Nodes aren't the same,
      // or else it'll get super confusing when we change them!
      .then(async root => {
        if ((!this[_updateAll] && !this[_global] && !root.meta.loadedFromDisk) || (this[_global] && this[_updateNames].length)) {
          await new this.constructor(this.options).loadActual({ root })
          const tree = root.target
          // even though we didn't load it from a package-lock.json FILE,
          // we still loaded it "from disk", meaning we have to reset
          // dep flags before assuming that any mutations were reflected.
          if (tree.children.size) {
            root.meta.loadedFromDisk = true
            // set these so that we don't try to ancient lockfile reload it
            root.meta.originalLockfileVersion = defaultLockfileVersion
            root.meta.lockfileVersion = defaultLockfileVersion
          }
        }
        root.meta.inferFormattingOptions(root.package)
        return root
      })

      .then(tree => {
        // null the virtual tree, because we're about to hack away at it
        // if you want another one, load another copy.
        this.idealTree = tree
        this.virtualTree = null
        process.emit('timeEnd', 'idealTree:init')
      })
  }

  async [_globalRootNode] () {
    const root = await this[_rootNodeFromPackage]({ dependencies: {} })
    // this is a gross kludge to handle the fact that we don't save
    // metadata on the root node in global installs, because the "root"
    // node is something like /usr/local/lib.
    const meta = new Shrinkwrap({
      path: this.path,
      lockfileVersion: this.options.lockfileVersion,
    })
    meta.reset()
    root.meta = meta
    return root
  }

  async [_rootNodeFromPackage] (pkg) {
    // if the path doesn't exist, then we explode at this point. Note that
    // this is not a problem for reify(), since it creates the root path
    // before ever loading trees.
    // TODO: make buildIdealTree() and loadActual handle a missing root path,
    // or a symlink to a missing target, and let reify() create it as needed.
    const real = await realpath(this.path, this[_rpcache], this[_stcache])
    const Cls = real === this.path ? Node : Link
    const root = new Cls({
      path: this.path,
      realpath: real,
      pkg,
      extraneous: false,
      dev: false,
      devOptional: false,
      peer: false,
      optional: false,
      global: this[_global],
      legacyPeerDeps: this.legacyPeerDeps,
      loadOverrides: true,
    })
    if (root.isLink) {
      root.target = new Node({
        path: real,
        realpath: real,
        pkg,
        extraneous: false,
        dev: false,
        devOptional: false,
        peer: false,
        optional: false,
        global: this[_global],
        legacyPeerDeps: this.legacyPeerDeps,
        root,
      })
    }
    return root
  }

  // process the add/rm requests by modifying the root node, and the
  // update.names request by queueing nodes dependent on those named.
  async [_applyUserRequests] (options) {
    process.emit('time', 'idealTree:userRequests')
    const tree = this.idealTree.target

    if (!this[_workspaces].length) {
      await this[_applyUserRequestsToNode](tree, options)
    } else {
      const nodes = this.workspaceNodes(tree, this[_workspaces])
      if (this[_includeWorkspaceRoot]) {
        nodes.push(tree)
      }
      const appliedRequests = nodes.map(
        node => this[_applyUserRequestsToNode](node, options)
      )
      await Promise.all(appliedRequests)
    }

    process.emit('timeEnd', 'idealTree:userRequests')
  }

  async [_applyUserRequestsToNode] (tree, options) {
    // If we have a list of package names to update, and we know it's
    // going to update them wherever they are, add any paths into those
    // named nodes to the buildIdealTree queue.
    if (!this[_global] && this[_updateNames].length) {
      this[_queueNamedUpdates]()
    }

    // global updates only update the globalTop nodes, but we need to know
    // that they're there, and not reinstall the world unnecessarily.
    const globalExplicitUpdateNames = []
    if (this[_global] && (this[_updateAll] || this[_updateNames].length)) {
      const nm = resolve(this.path, 'node_modules')
      for (const name of await readdir(nm).catch(() => [])) {
        tree.package.dependencies = tree.package.dependencies || {}
        const updateName = this[_updateNames].includes(name)
        if (this[_updateAll] || updateName) {
          if (updateName) {
            globalExplicitUpdateNames.push(name)
          }
          const dir = resolve(nm, name)
          const st = await lstat(dir)
            .catch(/* istanbul ignore next */ er => null)
          if (st && st.isSymbolicLink()) {
            const target = await readlink(dir)
            const real = resolve(dirname(dir), target)
            tree.package.dependencies[name] = `file:${real}`
          } else {
            tree.package.dependencies[name] = '*'
          }
        }
      }
    }

    if (this.auditReport && this.auditReport.size > 0) {
      await this[_queueVulnDependents](options)
    }

    const { add, rm } = options

    if (rm && rm.length) {
      addRmPkgDeps.rm(tree.package, rm)
      for (const name of rm) {
        this[_explicitRequests].add({ from: tree, name, action: 'DELETE' })
      }
    }

    if (add && add.length) {
      await this[_add](tree, options)
    }

    // triggers a refresh of all edgesOut.  this has to be done BEFORE
    // adding the edges to explicitRequests, because the package setter
    // resets all edgesOut.
    if (add && add.length || rm && rm.length || this[_global]) {
      tree.package = tree.package
    }

    for (const spec of this[_resolvedAdd]) {
      if (spec.tree === tree) {
        this[_explicitRequests].add(tree.edgesOut.get(spec.name))
      }
    }
    for (const name of globalExplicitUpdateNames) {
      this[_explicitRequests].add(tree.edgesOut.get(name))
    }

    this[_depsQueue].push(tree)
  }

  // This returns a promise because we might not have the name yet,
  // and need to call pacote.manifest to find the name.
  [_add] (tree, { add, saveType = null, saveBundle = false }) {
    // get the name for each of the specs in the list.
    // ie, doing `foo@bar` we just return foo
    // but if it's a url or git, we don't know the name until we
    // fetch it and look in its manifest.
    return Promise.all(add.map(async rawSpec => {
      // We do NOT provide the path to npa here, because user-additions
      // need to be resolved relative to the CWD the user is in.
      const spec = await this[_retrieveSpecName](npa(rawSpec))
        .then(spec => this[_updateFilePath](spec))
        .then(spec => this[_followSymlinkPath](spec))
      spec.tree = tree
      return spec
    })).then(add => {
      this[_resolvedAdd].push(...add)
      // now add is a list of spec objects with names.
      // find a home for each of them!
      addRmPkgDeps.add({
        pkg: tree.package,
        add,
        saveBundle,
        saveType,
        path: this.path,
        log: this.log,
      })
    })
  }

  async [_retrieveSpecName] (spec) {
    // if it's just @'' then we reload whatever's there, or get latest
    // if it's an explicit tag, we need to install that specific tag version
    const isTag = spec.rawSpec && spec.type === 'tag'

    if (spec.name && !isTag) {
      return spec
    }

    const mani = await pacote.manifest(spec, { ...this.options })
    // if it's a tag type, then we need to run it down to an actual version
    if (isTag) {
      return npa(`${mani.name}@${mani.version}`)
    }

    spec.name = mani.name
    return spec
  }

  async [_updateFilePath] (spec) {
    if (spec.type === 'file') {
      return this[_getRelpathSpec](spec, spec.fetchSpec)
    }

    return spec
  }

  async [_followSymlinkPath] (spec) {
    if (spec.type === 'directory') {
      const real = await (
        realpath(spec.fetchSpec, this[_rpcache], this[_stcache])
          // TODO: create synthetic test case to simulate realpath failure
          .catch(/* istanbul ignore next */() => null)
      )

      return this[_getRelpathSpec](spec, real)
    }
    return spec
  }

  [_getRelpathSpec] (spec, filepath) {
    /* istanbul ignore else - should also be covered by realpath failure */
    if (filepath) {
      const { name } = spec
      const tree = this.idealTree.target
      spec = npa(`file:${relpath(tree.path, filepath)}`, tree.path)
      spec.name = name
    }
    return spec
  }

  // TODO: provide a way to fix bundled deps by exposing metadata about
  // what's in the bundle at each published manifest.  Without that, we
  // can't possibly fix bundled deps without breaking a ton of other stuff,
  // and leaving the user subject to getting it overwritten later anyway.
  async [_queueVulnDependents] (options) {
    for (const vuln of this.auditReport.values()) {
      for (const node of vuln.nodes) {
        const bundler = node.getBundler()

        // XXX this belongs in the audit report itself, not here.
        // We shouldn't even get these things here, and they shouldn't
        // be printed by npm-audit-report as if they can be fixed, because
        // they can't.
        if (bundler) {
          this.log.warn(`audit fix ${node.name}@${node.version}`,
            `${node.location}\nis a bundled dependency of\n${
            bundler.name}@${bundler.version} at ${bundler.location}\n` +
            'It cannot be fixed automatically.\n' +
            `Check for updates to the ${bundler.name} package.`)
          continue
        }

        for (const edge of node.edgesIn) {
          this.addTracker('idealTree', edge.from.name, edge.from.location)
          this[_depsQueue].push(edge.from)
        }
      }
    }

    // note any that can't be fixed at the root level without --force
    // if there's a fix, we use that.  otherwise, the user has to remove it,
    // find a different thing, fix the upstream, etc.
    //
    // XXX: how to handle top nodes that aren't the root?  Maybe the report
    // just tells the user to cd into that directory and fix it?
    if (this[_force] && this.auditReport && this.auditReport.topVulns.size) {
      options.add = options.add || []
      options.rm = options.rm || []
      const nodesTouched = new Set()
      for (const [name, topVuln] of this.auditReport.topVulns.entries()) {
        const {
          simpleRange,
          topNodes,
          fixAvailable,
        } = topVuln
        for (const node of topNodes) {
          if (!node.isProjectRoot && !node.isWorkspace) {
            // not something we're going to fix, sorry.  have to cd into
            // that directory and fix it yourself.
            this.log.warn('audit', 'Manual fix required in linked project ' +
              `at ./${node.location} for ${name}@${simpleRange}.\n` +
              `'cd ./${node.location}' and run 'npm audit' for details.`)
            continue
          }

          if (!fixAvailable) {
            this.log.warn('audit', `No fix available for ${name}@${simpleRange}`)
            continue
          }

          const { isSemVerMajor, version } = fixAvailable
          const breakingMessage = isSemVerMajor
            ? 'a SemVer major change'
            : 'outside your stated dependency range'
          this.log.warn('audit', `Updating ${name} to ${version},` +
            `which is ${breakingMessage}.`)

          await this[_add](node, { add: [`${name}@${version}`] })
          nodesTouched.add(node)
        }
      }
      for (const node of nodesTouched) {
        node.package = node.package
      }
    }
  }

  [_isVulnerable] (node) {
    return this.auditReport && this.auditReport.isVulnerable(node)
  }

  [_avoidRange] (name) {
    if (!this.auditReport) {
      return null
    }
    const vuln = this.auditReport.get(name)
    if (!vuln) {
      return null
    }
    return vuln.range
  }

  [_queueNamedUpdates] () {
    // ignore top nodes, since they are not loaded the same way, and
    // probably have their own project associated with them.

    // for every node with one of the names on the list, we add its
    // dependents to the queue to be evaluated.  in buildDepStep,
    // anything on the update names list will get refreshed, even if
    // it isn't a problem.

    // XXX this could be faster by doing a series of inventory.query('name')
    // calls rather than walking over everything in the tree.
    const set = this.idealTree.inventory
      .filter(n => this[_shouldUpdateNode](n))
    // XXX add any invalid edgesOut to the queue
    for (const node of set) {
      for (const edge of node.edgesIn) {
        this.addTracker('idealTree', edge.from.name, edge.from.location)
        this[_depsQueue].push(edge.from)
      }
    }
  }

  [_shouldUpdateNode] (node) {
    return this[_updateNames].includes(node.name) &&
      !node.isTop &&
      !node.inDepBundle &&
      !node.inShrinkwrap
  }

  async [_inflateAncientLockfile] () {
    const { meta, inventory } = this.idealTree
    const ancient = meta.ancientLockfile
    const old = meta.loadedFromDisk && !(meta.originalLockfileVersion >= 2)

    if (inventory.size === 0 || !ancient && !old) {
      return
    }

    // if the lockfile is from node v5 or earlier, then we'll have to reload
    // all the manifests of everything we encounter.  this is costly, but at
    // least it's just a one-time hit.
    process.emit('time', 'idealTree:inflate')

    // don't warn if we're not gonna actually write it back anyway.
    const heading = ancient ? 'ancient lockfile' : 'old lockfile'
    if (ancient || !this.options.lockfileVersion ||
        this.options.lockfileVersion >= defaultLockfileVersion) {
      this.log.warn(heading,
        `
The ${meta.type} file was created with an old version of npm,
so supplemental metadata must be fetched from the registry.

This is a one-time fix-up, please be patient...
`)
    }

    this.addTracker('idealTree:inflate')
    const queue = []
    for (const node of inventory.values()) {
      if (node.isProjectRoot) {
        continue
      }

      queue.push(async () => {
        this.log.silly('inflate', node.location)
        const { resolved, version, path, name, location, integrity } = node
        // don't try to hit the registry for linked deps
        const useResolved = resolved && (
          !version || resolved.startsWith('file:')
        )
        const id = useResolved ? resolved
          : version || `file:${node.path}`
        const spec = npa.resolve(name, id, dirname(path))
        const sloc = location.substr('node_modules/'.length)
        const t = `idealTree:inflate:${sloc}`
        this.addTracker(t)
        await pacote.manifest(spec, {
          ...this.options,
          resolved: resolved,
          integrity: integrity,
          fullMetadata: false,
        }).then(mani => {
          node.package = { ...mani, _id: `${mani.name}@${mani.version}` }
        }).catch((er) => {
          const warning = `Could not fetch metadata for ${name}@${id}`
          this.log.warn(heading, warning, er)
        })
        this.finishTracker(t)
      })
    }
    await promiseCallLimit(queue)

    // have to re-calc dep flags, because the nodes don't have edges
    // until their packages get assigned, so everything looks extraneous
    calcDepFlags(this.idealTree)

    // yes, yes, this isn't the "original" version, but now that it's been
    // upgraded, we need to make sure we don't do the work to upgrade it
    // again, since it's now as new as can be.
    meta.originalLockfileVersion = defaultLockfileVersion
    this.finishTracker('idealTree:inflate')
    process.emit('timeEnd', 'idealTree:inflate')
  }

  // at this point we have a virtual tree with the actual root node's
  // package deps, which may be partly or entirely incomplete, invalid
  // or extraneous.
  [_buildDeps] () {
    process.emit('time', 'idealTree:buildDeps')
    const tree = this.idealTree.target
    tree.assertRootOverrides()
    this[_depsQueue].push(tree)
    // XXX also push anything that depends on a node with a name
    // in the override list
    this.log.silly('idealTree', 'buildDeps')
    this.addTracker('idealTree', tree.name, '')
    return this[_buildDepStep]()
      .then(() => process.emit('timeEnd', 'idealTree:buildDeps'))
  }

  async [_buildDepStep] () {
    // removes tracker of previous dependency in the queue
    if (this[_currentDep]) {
      const { location, name } = this[_currentDep]
      process.emit('timeEnd', `idealTree:${location || '#root'}`)
      this.finishTracker('idealTree', name, location)
      this[_currentDep] = null
    }

    if (!this[_depsQueue].length) {
      return this[_resolveLinks]()
    }

    // sort physically shallower deps up to the front of the queue,
    // because they'll affect things deeper in, then alphabetical
    this[_depsQueue].sort((a, b) =>
      (a.depth - b.depth) || localeCompare(a.path, b.path))

    const node = this[_depsQueue].shift()
    const bd = node.package.bundleDependencies
    const hasBundle = bd && Array.isArray(bd) && bd.length
    const { hasShrinkwrap } = node

    // if the node was already visited, or has since been removed from the
    // tree, skip over it and process the rest of the queue.  If a node has
    // a shrinkwrap, also skip it, because it's going to get its deps
    // satisfied by whatever's in that file anyway.
    if (this[_depsSeen].has(node) ||
        node.root !== this.idealTree ||
        hasShrinkwrap && !this[_complete]) {
      return this[_buildDepStep]()
    }

    this[_depsSeen].add(node)
    this[_currentDep] = node
    process.emit('time', `idealTree:${node.location || '#root'}`)

    // if we're loading a _complete_ ideal tree, for a --package-lock-only
    // installation for example, we have to crack open the tarball and
    // look inside if it has bundle deps or shrinkwraps.  note that this is
    // not necessary during a reification, because we just update the
    // ideal tree by reading bundles/shrinkwraps in place.
    // Don't bother if the node is from the actual tree and hasn't
    // been resolved, because we can't fetch it anyway, could be anything!
    const crackOpen = this[_complete] &&
      node !== this.idealTree &&
      node.resolved &&
      (hasBundle || hasShrinkwrap)
    if (crackOpen) {
      const Arborist = this.constructor
      const opt = { ...this.options }
      await cacache.tmp.withTmp(this.cache, opt, async path => {
        await pacote.extract(node.resolved, path, {
          ...opt,
          resolved: node.resolved,
          integrity: node.integrity,
        })

        if (hasShrinkwrap) {
          await new Arborist({ ...this.options, path })
            .loadVirtual({ root: node })
        }

        if (hasBundle) {
          await new Arborist({ ...this.options, path })
            .loadActual({ root: node, ignoreMissing: true })
        }
      })
    }

    // if any deps are missing or invalid, then we fetch the manifest for
    // the thing we want, and build a new dep node from that.
    // Then, find the ideal placement for that node.  The ideal placement
    // searches from the node's deps (or parent deps in the case of non-root
    // peer deps), and walks up the tree until it finds the highest spot
    // where it doesn't cause any conflicts.
    //
    // A conflict can be:
    // - A node by that name already exists at that location.
    // - The parent has a peer dep on that name
    // - One of the node's peer deps conflicts at that location, unless the
    //   peer dep is met by a node at that location, which is fine.
    //
    // If we create a new node, then build its ideal deps as well.
    //
    // Note: this is the same "maximally naive" deduping tree-building
    // algorithm that npm has used since v3.  In a case like this:
    //
    // root -> (a@1, b@1||2)
    // a -> (b@1)
    //
    // You'll end up with a tree like this:
    //
    // root
    // +-- a@1
    // |   +-- b@1
    // +-- b@2
    //
    // rather than this, more deduped, but just as correct tree:
    //
    // root
    // +-- a@1
    // +-- b@1
    //
    // Another way to look at it is that this algorithm favors getting higher
    // version deps at higher levels in the tree, even if that reduces
    // potential deduplication.
    //
    // Set `preferDedupe: true` in the options to replace the shallower
    // dep if allowed.

    const tasks = []
    const peerSource = this[_peerSetSource].get(node) || node
    for (const edge of this[_problemEdges](node)) {
      if (edge.peerConflicted) {
        continue
      }

      // peerSetSource is only relevant when we have a peerEntryEdge
      // otherwise we're setting regular non-peer deps as if they have
      // a virtual root of whatever brought in THIS node.
      // so we VR the node itself if the edge is not a peer
      const source = edge.peer ? peerSource : node

      const virtualRoot = this[_virtualRoot](source, true)
      // reuse virtual root if we already have one, but don't
      // try to do the override ahead of time, since we MAY be able
      // to create a more correct tree than the virtual root could.
      const vrEdge = virtualRoot && virtualRoot.edgesOut.get(edge.name)
      const vrDep = vrEdge && vrEdge.valid && vrEdge.to
      // only re-use the virtualRoot if it's a peer edge we're placing.
      // otherwise, we end up in situations where we override peer deps that
      // we could have otherwise found homes for.  Eg:
      // xy -> (x, y)
      // x -> PEER(z@1)
      // y -> PEER(z@2)
      // If xy is a dependency, we can resolve this like:
      // project
      // +-- xy
      // |   +-- y
      // |   +-- z@2
      // +-- x
      // +-- z@1
      // But if x and y are loaded in the same virtual root, then they will
      // be forced to agree on a version of z.
      const required = new Set([edge.from])
      const parent = edge.peer ? virtualRoot : null
      const dep = vrDep && vrDep.satisfies(edge) ? vrDep
        : await this[_nodeFromEdge](edge, parent, null, required)

      /* istanbul ignore next */
      debug(() => {
        if (!dep) {
          throw new Error('no dep??')
        }
      })

      tasks.push({ edge, dep })
    }

    const placeDeps = tasks
      .sort((a, b) => localeCompare(a.edge.name, b.edge.name))
      .map(({ edge, dep }) => new PlaceDep({
        edge,
        dep,

        explicitRequest: this[_explicitRequests].has(edge),
        updateNames: this[_updateNames],
        auditReport: this.auditReport,
        force: this[_force],
        preferDedupe: this[_preferDedupe],
        legacyBundling: this[_legacyBundling],
        strictPeerDeps: this[_strictPeerDeps],
        legacyPeerDeps: this.legacyPeerDeps,
        globalStyle: this[_globalStyle],
      }))

    const promises = []
    for (const pd of placeDeps) {
      // placing a dep is actually a tree of placing the dep itself
      // and all of its peer group that aren't already met by the tree
      depth({
        tree: pd,
        getChildren: pd => pd.children,
        visit: pd => {
          const { placed, edge, canPlace: cpd } = pd
          // if we didn't place anything, nothing to do here
          if (!placed) {
            return
          }

          // we placed something, that means we changed the tree
          if (placed.errors.length) {
            this[_loadFailures].add(placed)
          }
          this[_mutateTree] = true
          if (cpd.canPlaceSelf === OK) {
            for (const edgeIn of placed.edgesIn) {
              if (edgeIn === edge) {
                continue
              }
              const { from, valid, peerConflicted } = edgeIn
              if (!peerConflicted && !valid && !this[_depsSeen].has(from)) {
                this.addTracker('idealTree', from.name, from.location)
                this[_depsQueue].push(edgeIn.from)
              }
            }
          } else {
            /* istanbul ignore else - should be only OK or REPLACE here */
            if (cpd.canPlaceSelf === REPLACE) {
              // this may also create some invalid edges, for example if we're
              // intentionally causing something to get nested which was
              // previously placed in this location.
              for (const edgeIn of placed.edgesIn) {
                if (edgeIn === edge) {
                  continue
                }

                const { valid, peerConflicted } = edgeIn
                if (!valid && !peerConflicted) {
                  // if it's already been visited, we have to re-visit
                  // otherwise, just enqueue normally.
                  this[_depsSeen].delete(edgeIn.from)
                  this[_depsQueue].push(edgeIn.from)
                }
              }
            }
          }

          /* istanbul ignore if - should be impossible */
          if (cpd.canPlaceSelf === CONFLICT) {
            debug(() => {
              const er = new Error('placed with canPlaceSelf=CONFLICT')
              throw Object.assign(er, { placeDep: pd })
            })
            return
          }

          // lastly, also check for the missing deps of the node we placed,
          // and any holes created by pruning out conflicted peer sets.
          this[_depsQueue].push(placed)
          for (const dep of pd.needEvaluation) {
            this[_depsSeen].delete(dep)
            this[_depsQueue].push(dep)
          }

          // pre-fetch any problem edges, since we'll need these soon
          // if it fails at this point, though, dont' worry because it
          // may well be an optional dep that has gone missing.  it'll
          // fail later anyway.
          const from = fromPath(placed)
          promises.push(...this[_problemEdges](placed).map(e =>
            this[_fetchManifest](npa.resolve(e.name, e.spec, from))
              .catch(er => null)))
        },
      })
    }

    for (const { to } of node.edgesOut.values()) {
      if (to && to.isLink && to.target) {
        this[_linkNodes].add(to)
      }
    }

    await Promise.all(promises)
    return this[_buildDepStep]()
  }

  // loads a node from an edge, and then loads its peer deps (and their
  // peer deps, on down the line) into a virtual root parent.
  async [_nodeFromEdge] (edge, parent_, secondEdge, required) {
    // create a virtual root node with the same deps as the node that
    // is requesting this one, so that we can get all the peer deps in
    // a context where they're likely to be resolvable.
    // Note that the virtual root will also have virtual copies of the
    // targets of any child Links, so that they resolve appropriately.
    const parent = parent_ || this[_virtualRoot](edge.from)

    const spec = npa.resolve(edge.name, edge.spec, edge.from.path)
    const first = await this[_nodeFromSpec](edge.name, spec, parent, edge)

    // we might have a case where the parent has a peer dependency on
    // `foo@*` which resolves to v2, but another dep in the set has a
    // peerDependency on `foo@1`.  In that case, if we force it to be v2,
    // we're unnecessarily triggering an ERESOLVE.
    // If we have a second edge to worry about, and it's not satisfied
    // by the first node, try a second and see if that satisfies the
    // original edge here.
    const spec2 = secondEdge && npa.resolve(
      edge.name,
      secondEdge.spec,
      secondEdge.from.path
    )
    const second = secondEdge && !secondEdge.valid
      ? await this[_nodeFromSpec](edge.name, spec2, parent, secondEdge)
      : null

    // pick the second one if they're both happy with that, otherwise first
    const node = second && edge.valid ? second : first
    // ensure the one we want is the one that's placed
    node.parent = parent

    if (required.has(edge.from) && edge.type !== 'peerOptional' ||
        secondEdge && (
          required.has(secondEdge.from) && secondEdge.type !== 'peerOptional')) {
      required.add(node)
    }

    // keep track of the thing that caused this node to be included.
    const src = parent.sourceReference
    this[_peerSetSource].set(node, src)

    // do not load the peers along with the set if this is a global top pkg
    // otherwise we'll be tempted to put peers as other top-level installed
    // things, potentially clobbering what's there already, which is not
    // what we want.  the missing edges will be picked up on the next pass.
    if (this[_global] && edge.from.isProjectRoot) {
      return node
    }

    // otherwise, we have to make sure that our peers can go along with us.
    return this[_loadPeerSet](node, required)
  }

  [_virtualRoot] (node, reuse = false) {
    if (reuse && this[_virtualRoots].has(node)) {
      return this[_virtualRoots].get(node)
    }

    const vr = new Node({
      path: node.realpath,
      sourceReference: node,
      legacyPeerDeps: this.legacyPeerDeps,
      overrides: node.overrides,
    })

    // also need to set up any targets from any link deps, so that
    // they are properly reflected in the virtual environment
    for (const child of node.children.values()) {
      if (child.isLink) {
        new Node({
          path: child.realpath,
          sourceReference: child.target,
          root: vr,
        })
      }
    }

    this[_virtualRoots].set(node, vr)
    return vr
  }

  [_problemEdges] (node) {
    // skip over any bundled deps, they're not our problem.
    // Note that this WILL fetch bundled meta-deps which are also dependencies
    // but not listed as bundled deps.  When reifying, we first unpack any
    // nodes that have bundleDependencies, then do a loadActual on them, move
    // the nodes into the ideal tree, and then prune.  So, fetching those
    // possibly-bundled meta-deps at this point doesn't cause any worse
    // problems than a few unnecessary packument fetches.

    // also skip over any nodes in the tree that failed to load, since those
    // will crash the install later on anyway.
    const bd = node.isProjectRoot || node.isWorkspace ? null
      : node.package.bundleDependencies
    const bundled = new Set(bd || [])

    return [...node.edgesOut.values()]
      .filter(edge => {
        // If it's included in a bundle, we take whatever is specified.
        if (bundled.has(edge.name)) {
          return false
        }

        // If it's already been logged as a load failure, skip it.
        if (edge.to && this[_loadFailures].has(edge.to)) {
          return false
        }

        // If it's shrinkwrapped, we use what the shrinkwap wants.
        if (edge.to && edge.to.inShrinkwrap) {
          return false
        }

        // If the edge has no destination, that's a problem, unless
        // if it's peerOptional and not explicitly requested.
        if (!edge.to) {
          return edge.type !== 'peerOptional' ||
            this[_explicitRequests].has(edge)
        }

        // If the edge has an error, there's a problem.
        if (!edge.valid) {
          return true
        }

        // If the edge is a workspace, and it's valid, leave it alone
        if (edge.to.isWorkspace) {
          return false
        }

        // user explicitly asked to update this package by name, problem
        if (this[_updateNames].includes(edge.name)) {
          return true
        }

        // fixing a security vulnerability with this package, problem
        if (this[_isVulnerable](edge.to)) {
          return true
        }

        // user has explicitly asked to install this package, problem
        if (this[_explicitRequests].has(edge)) {
          return true
        }

        // No problems!
        return false
      })
  }

  async [_fetchManifest] (spec) {
    const options = {
      ...this.options,
      avoid: this[_avoidRange](spec.name),
    }
    // get the intended spec and stored metadata from yarn.lock file,
    // if available and valid.
    spec = this.idealTree.meta.checkYarnLock(spec, options)

    if (this[_manifests].has(spec.raw)) {
      return this[_manifests].get(spec.raw)
    } else {
      this.log.silly('fetch manifest', spec.raw)
      const p = pacote.manifest(spec, options)
        .then(mani => {
          this[_manifests].set(spec.raw, mani)
          return mani
        })
      this[_manifests].set(spec.raw, p)
      return p
    }
  }

  [_nodeFromSpec] (name, spec, parent, edge) {
    // pacote will slap integrity on its options, so we have to clone
    // the object so it doesn't get mutated.
    // Don't bother to load the manifest for link deps, because the target
    // might be within another package that doesn't exist yet.
    const { legacyPeerDeps } = this
    return spec.type === 'directory'
      ? this[_linkFromSpec](name, spec, parent, edge)
      : this[_fetchManifest](spec)
        .then(pkg => new Node({ name, pkg, parent, legacyPeerDeps }), error => {
          error.requiredBy = edge.from.location || '.'

          // failed to load the spec, either because of enotarget or
          // fetch failure of some other sort.  save it so we can verify
          // later that it's optional, otherwise the error is fatal.
          const n = new Node({
            name,
            parent,
            error,
            legacyPeerDeps,
          })
          this[_loadFailures].add(n)
          return n
        })
  }

  [_linkFromSpec] (name, spec, parent, edge) {
    const realpath = spec.fetchSpec
    const { legacyPeerDeps } = this
    return rpj(realpath + '/package.json').catch(() => ({})).then(pkg => {
      const link = new Link({ name, parent, realpath, pkg, legacyPeerDeps })
      this[_linkNodes].add(link)
      return link
    })
  }

  // load all peer deps and meta-peer deps into the node's parent
  // At the end of this, the node's peer-type outward edges are all
  // resolved, and so are all of theirs, but other dep types are not.
  // We prefer to get peer deps that meet the requiring node's dependency,
  // if possible, since that almost certainly works (since that package was
  // developed with this set of deps) and will typically be more restrictive.
  // Note that the peers in the set can conflict either with each other,
  // or with a direct dependency from the virtual root parent!  In strict
  // mode, this is always an error.  In force mode, it never is, and we
  // prefer the parent's non-peer dep over a peer dep, or the version that
  // gets placed first.  In non-strict mode, we behave strictly if the
  // virtual root is based on the root project, and allow non-peer parent
  // deps to override, but throw if no preference can be determined.
  async [_loadPeerSet] (node, required) {
    const peerEdges = [...node.edgesOut.values()]
      // we typically only install non-optional peers, but we have to
      // factor them into the peerSet so that we can avoid conflicts
      .filter(e => e.peer && !(e.valid && e.to))
      .sort(({ name: a }, { name: b }) => localeCompare(a, b))

    for (const edge of peerEdges) {
      // already placed this one, and we're happy with it.
      if (edge.valid && edge.to) {
        continue
      }

      const parentEdge = node.parent.edgesOut.get(edge.name)
      const { isProjectRoot, isWorkspace } = node.parent.sourceReference
      const isMine = isProjectRoot || isWorkspace
      const conflictOK = this[_force] || !isMine && !this[_strictPeerDeps]

      if (!edge.to) {
        if (!parentEdge) {
          // easy, just put the thing there
          await this[_nodeFromEdge](edge, node.parent, null, required)
          continue
        } else {
          // if the parent's edge is very broad like >=1, and the edge in
          // question is something like 1.x, then we want to get a 1.x, not
          // a 2.x.  pass along the child edge as an advisory guideline.
          // if the parent edge doesn't satisfy the child edge, and the
          // child edge doesn't satisfy the parent edge, then we have
          // a conflict.  this is always a problem in strict mode, never
          // in force mode, and a problem in non-strict mode if this isn't
          // on behalf of our project.  in all such cases, we warn at least.
          const dep = await this[_nodeFromEdge](
            parentEdge,
            node.parent,
            edge,
            required
          )

          // hooray! that worked!
          if (edge.valid) {
            continue
          }

          // allow it.  either we're overriding, or it's not something
          // that will be installed by default anyway, and we'll fail when
          // we get to the point where we need to, if we need to.
          if (conflictOK || !required.has(dep)) {
            edge.peerConflicted = true
            continue
          }

          // problem
          this[_failPeerConflict](edge, parentEdge)
        }
      }

      // There is something present already, and we're not happy about it
      // See if the thing we WOULD be happy with is also going to satisfy
      // the other dependents on the current node.
      const current = edge.to
      const dep = await this[_nodeFromEdge](edge, null, null, required)
      if (dep.canReplace(current)) {
        await this[_nodeFromEdge](edge, node.parent, null, required)
        continue
      }

      // at this point we know that there is a dep there, and
      // we don't like it.  always fail strictly, always allow forcibly or
      // in non-strict mode if it's not our fault.  don't warn here, because
      // we are going to warn again when we place the deps, if we end up
      // overriding for something else.  If the thing that has this dep
      // isn't also required, then there's a good chance we won't need it,
      // so allow it for now and let it conflict if it turns out to actually
      // be necessary for the installation.
      if (conflictOK || !required.has(edge.from)) {
        continue
      }

      // ok, it's the root, or we're in unforced strict mode, so this is bad
      this[_failPeerConflict](edge, parentEdge)
    }
    return node
  }

  [_failPeerConflict] (edge, currentEdge) {
    const expl = this[_explainPeerConflict](edge, currentEdge)
    throw Object.assign(new Error('unable to resolve dependency tree'), expl)
  }

  [_explainPeerConflict] (edge, currentEdge) {
    const node = edge.from
    const curNode = node.resolve(edge.name)
    const current = curNode.explain()
    return {
      code: 'ERESOLVE',
      current,
      // it SHOULD be impossible to get here without a current node in place,
      // but this at least gives us something report on when bugs creep into
      // the tree handling logic.
      currentEdge: currentEdge ? currentEdge.explain() : null,
      edge: edge.explain(),
      strictPeerDeps: this[_strictPeerDeps],
      force: this[_force],
    }
  }

  // go through all the links in the this[_linkNodes] set
  // for each one:
  // - if outside the root, ignore it, assume it's fine, it's not our problem
  // - if a node in the tree already, assign the target to that node.
  // - if a path under an existing node, then assign that as the fsParent,
  //   and add it to the _depsQueue
  //
  // call buildDepStep if anything was added to the queue, otherwise we're done
  [_resolveLinks] () {
    for (const link of this[_linkNodes]) {
      this[_linkNodes].delete(link)

      // link we never ended up placing, skip it
      if (link.root !== this.idealTree) {
        continue
      }

      const tree = this.idealTree.target
      const external = !link.target.isDescendantOf(tree)

      // outside the root, somebody else's problem, ignore it
      if (external && !this[_follow]) {
        continue
      }

      // didn't find a parent for it or it has not been seen yet
      // so go ahead and process it.
      const unseenLink = (link.target.parent || link.target.fsParent) &&
        !this[_depsSeen].has(link.target)

      if (this[_follow] &&
          !link.target.parent &&
          !link.target.fsParent ||
          unseenLink) {
        this.addTracker('idealTree', link.target.name, link.target.location)
        this[_depsQueue].push(link.target)
      }
    }

    if (this[_depsQueue].length) {
      return this[_buildDepStep]()
    }
  }

  [_fixDepFlags] () {
    process.emit('time', 'idealTree:fixDepFlags')
    const metaFromDisk = this.idealTree.meta.loadedFromDisk
    const flagsSuspect = this[_flagsSuspect]
    const mutateTree = this[_mutateTree]
    // if the options set prune:false, then we don't prune, but we still
    // mark the extraneous items in the tree if we modified it at all.
    // If we did no modifications, we just iterate over the extraneous nodes.
    // if we started with an empty tree, then the dep flags are already
    // all set to true, and there can be nothing extraneous, so there's
    // nothing to prune, because we built it from scratch.  if we didn't
    // add or remove anything, then also nothing to do.
    if (metaFromDisk && mutateTree) {
      resetDepFlags(this.idealTree)
    }

    // update all the dev/optional/etc flags in the tree
    // either we started with a fresh tree, or we
    // reset all the flags to find the extraneous nodes.
    //
    // if we started from a blank slate, or changed something, then
    // the dep flags will be all set to true.
    if (!metaFromDisk || mutateTree) {
      calcDepFlags(this.idealTree)
    } else {
      // otherwise just unset all the flags on the root node
      // since they will sometimes have the default value
      this.idealTree.extraneous = false
      this.idealTree.dev = false
      this.idealTree.optional = false
      this.idealTree.devOptional = false
      this.idealTree.peer = false
    }

    // at this point, any node marked as extraneous should be pruned.
    // if we started from a shrinkwrap, and then added/removed something,
    // then the tree is suspect.  Prune what is marked as extraneous.
    // otherwise, don't bother.
    const needPrune = metaFromDisk && (mutateTree || flagsSuspect)
    if (this[_prune] && needPrune) {
      this[_idealTreePrune]()
    }

    process.emit('timeEnd', 'idealTree:fixDepFlags')
  }

  [_idealTreePrune] () {
    for (const node of this.idealTree.inventory.filter(n => n.extraneous)) {
      node.parent = null
    }
  }

  [_pruneFailedOptional] () {
    for (const node of this[_loadFailures]) {
      if (!node.optional) {
        throw node.errors[0]
      }

      const set = optionalSet(node)
      for (const node of set) {
        node.parent = null
      }
    }
  }
}
