// mixin implementing the buildIdealTree method
const rpj = require('read-package-json-fast')
const npa = require('npm-package-arg')
const pacote = require('pacote')
const cacache = require('cacache')
const semver = require('semver')
const promiseCallLimit = require('promise-call-limit')
const getPeerSet = require('../peer-set.js')
const realpath = require('../../lib/realpath.js')

const fromPath = require('../from-path.js')
const calcDepFlags = require('../calc-dep-flags.js')
const Shrinkwrap = require('../shrinkwrap.js')
const Node = require('../node.js')
const Link = require('../link.js')
const addRmPkgDeps = require('../add-rm-pkg-deps.js')
const gatherDepSet = require('../gather-dep-set.js')
const optionalSet = require('../optional-set.js')
const {checkEngine, checkPlatform} = require('npm-install-checks')

// enum of return values for canPlaceDep.
// No, this is a conflict, you may not put that package here
const CONFLICT = Symbol('CONFLICT')
// Yes, this is fine, and should not be a problem
const OK = Symbol('OK')
// No need, because the package already here is fine
const KEEP = Symbol('KEEP')
// Yes, clobber the package that is already here
const REPLACE = Symbol('REPLACE')

const relpath = require('../relpath.js')

// note: some of these symbols are shared so we can hit
// them with unit tests and reuse them across mixins
const _complete = Symbol('complete')
const _depsSeen = Symbol('depsSeen')
const _depsQueue = Symbol('depsQueue')
const _currentDep = Symbol('currentDep')
const _updateAll = Symbol('updateAll')
const _mutateTree = Symbol('mutateTree')
const _flagsSuspect = Symbol.for('flagsSuspect')
const _prune = Symbol('prune')
const _preferDedupe = Symbol('preferDedupe')
const _legacyBundling = Symbol('legacyBundling')
const _parseSettings = Symbol('parseSettings')
const _initTree = Symbol('initTree')
const _applyUserRequests = Symbol('applyUserRequests')
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
const _placeDep = Symbol.for('placeDep')
const _canPlaceDep = Symbol.for('canPlaceDep')
const _canPlacePeers = Symbol('canPlacePeers')
const _pruneForReplacement = Symbol('pruneForReplacement')
const _fixDepFlags = Symbol('fixDepFlags')
const _resolveLinks = Symbol('resolveLinks')
const _rootNodeFromPackage = Symbol('rootNodeFromPackage')
const _add = Symbol('add')
const _resolvedAdd = Symbol.for('resolvedAdd')
const _queueNamedUpdates = Symbol('queueNamedUpdates')
const _queueVulnDependents = Symbol('queueVulnDependents')
const _avoidRange = Symbol('avoidRange')
const _shouldUpdateNode = Symbol('shouldUpdateNode')
const _resetDepFlags = Symbol('resetDepFlags')
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

// used for the ERESOLVE error to show the last peer conflict encountered
const _peerConflict = Symbol('peerConflict')

// used by Reify mixin
const _force = Symbol.for('force')
const _explicitRequests = Symbol.for('explicitRequests')
const _global = Symbol.for('global')
const _idealTreePrune = Symbol.for('idealTreePrune')

module.exports = cls => class IdealTreeBuilder extends cls {
  constructor (options) {
    super(options)

    // normalize trailing slash
    const registry = options.registry || 'https://registry.npmjs.org'
    options.registry = this.registry = registry.replace(/\/+$/, '') + '/'

    const {
      idealTree = null,
      global = false,
      follow = false,
      globalStyle = false,
      legacyPeerDeps = false,
      force = false,
      packageLock = true,
      strictPeerDeps = false,
    } = options

    this[_force] = !!force
    this[_strictPeerDeps] = !!strictPeerDeps

    this.idealTree = idealTree
    this.legacyPeerDeps = legacyPeerDeps

    this[_usePackageLock] = packageLock
    this[_global] = !!global
    this[_globalStyle] = this[_global] || globalStyle
    this[_follow] = !!follow

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
    this[_peerConflict] = null
  }

  get explicitRequests () {
    return new Set(this[_explicitRequests])
  }

  // public method
  async buildIdealTree (options = {}) {
    if (this.idealTree)
      return Promise.resolve(this.idealTree)

    // allow the user to set reify options on the ctor as well.
    // XXX: deprecate separate reify() options object.
    options = { ...this.options, ...options }

    // an empty array or any falsey value is the same as null
    if (!options.add || options.add.length === 0)
      options.add = null
    if (!options.rm || options.rm.length === 0)
      options.rm = null

    process.emit('time', 'idealTree')

    if (!options.add && !options.rm && this[_global])
      return Promise.reject(new Error('global requires an add or rm option'))

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
      await this[_applyUserRequests](options)
      await this[_inflateAncientLockfile]()
      await this[_buildDeps]()
      await this[_fixDepFlags]()
      await this[_pruneFailedOptional]()
      await this[_checkEngineAndPlatform]()
    } finally {
      process.emit('timeEnd', 'idealTree')
      this.finishTracker('idealTree')
    }

    return this.idealTree
  }

  [_checkEngineAndPlatform] () {
    // engine/platform checks throw, so start the promise chain off first
    return Promise.resolve()
      .then(() => {
        for (const node of this.idealTree.inventory.values()) {
          if (!node.optional) {
            this[_checkEngine](node)
            this[_checkPlatform](node)
          }
        }
      })
  }

  [_checkPlatform] (node) {
    checkPlatform(node.package, this[_force])
  }

  [_checkEngine] (node) {
    const { engineStrict, npmVersion, nodeVersion } = this.options
    const c = () => checkEngine(node.package, npmVersion, nodeVersion, this[_force])

    if (engineStrict)
      c()
    else {
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

    if (update.all || !Array.isArray(update.names))
      update.names = []

    this[_complete] = !!options.complete
    this[_preferDedupe] = !!options.preferDedupe
    this[_legacyBundling] = !!options.legacyBundling
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
          if (er.code === 'EJSONPARSE')
            throw er
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
        ? Shrinkwrap.reset({ path: this.path })
          .then(meta => Object.assign(root, {meta}))
        : this.loadVirtual({ root }))

      // if we don't have a lockfile to go from, then start with the
      // actual tree, so we only make the minimum required changes.
      // don't do this for global installs or updates, because in those
      // cases we don't use a lockfile anyway.
      // Load on a new Arborist object, so the Nodes aren't the same,
      // or else it'll get super confusing when we change them!
      // Only have to mapWorkspaces if we didn't get it from actual or virtual
      .then(async root => {
        if (!this[_updateAll] && !this[_global] && !root.meta.loadedFromDisk)
          await new this.constructor(this.options).loadActual({ root })
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

  [_globalRootNode] () {
    const root = this[_rootNodeFromPackage]({})
    // this is a gross kludge to handle the fact that we don't save
    // metadata on the root node in global installs, because the "root"
    // node is something like /usr/local/lib/node_modules.
    const meta = new Shrinkwrap({ path: this.path })
    meta.reset()
    root.meta = meta
    return Promise.resolve(root)
  }

  [_rootNodeFromPackage] (pkg) {
    return new Node({
      path: this.path,
      pkg,
      extraneous: false,
      dev: false,
      devOptional: false,
      peer: false,
      optional: false,
      global: this[_global],
      legacyPeerDeps: this.legacyPeerDeps,
    })
  }

  // process the add/rm requests by modifying the root node, and the
  // update.names request by queueing nodes dependent on those named.
  async [_applyUserRequests] (options) {
    process.emit('time', 'idealTree:userRequests')
    // If we have a list of package names to update, and we know it's
    // going to update them wherever they are, add any paths into those
    // named nodes to the buildIdealTree queue.
    if (this[_updateNames].length)
      this[_queueNamedUpdates]()

    if (this.auditReport && this.auditReport.size > 0)
      this[_queueVulnDependents](options)

    if (options.rm && options.rm.length) {
      addRmPkgDeps.rm(this.idealTree.package, options.rm)
      for (const name of options.rm)
        this[_explicitRequests].add(name)
    }

    if (options.add)
      await this[_add](options)

    // triggers a refresh of all edgesOut
    this.idealTree.package = this.idealTree.package
    process.emit('timeEnd', 'idealTree:userRequests')
  }

  // This returns a promise because we might not have the name yet,
  // and need to call pacote.manifest to find the name.
  [_add] ({add, saveType = null, saveBundle = false}) {
    // get the name for each of the specs in the list.
    // ie, doing `foo@bar` we just return foo
    // but if it's a url or git, we don't know the name until we
    // fetch it and look in its manifest.
    return Promise.all(add.map(rawSpec =>
      this[_retrieveSpecName](npa(rawSpec))
        .then(add => this[_updateFilePath](add))
        .then(add => this[_followSymlinkPath](add))
    )).then(add => {
      this[_resolvedAdd] = add
      // now add is a list of spec objects with names.
      // find a home for each of them!
      addRmPkgDeps.add({
        pkg: this.idealTree.package,
        add,
        saveBundle,
        saveType,
        path: this.path,
      })
      for (const spec of add)
        this[_explicitRequests].add(spec.name)
    })
  }

  async [_retrieveSpecName] (spec) {
    // if it's just @'' then we reload whatever's there, or get latest
    // if it's an explicit tag, we need to install that specific tag version
    const isTag = spec.rawSpec && spec.type === 'tag'

    if (spec.name && !isTag)
      return spec

    const mani = await pacote.manifest(spec, { ...this.options })
    // if it's a tag type, then we need to run it down to an actual version
    if (isTag)
      return npa(`${mani.name}@${mani.version}`)

    spec.name = mani.name
    return spec
  }

  async [_updateFilePath] (spec) {
    if (spec.type === 'file')
      spec = this[_getRelpathSpec](spec, spec.fetchSpec)

    return spec
  }

  async [_followSymlinkPath] (spec) {
    if (spec.type === 'directory') {
      const real = await (
        realpath(spec.fetchSpec, this[_rpcache], this[_stcache])
          // TODO: create synthetic test case to simulate realpath failure
          .catch(/* istanbul ignore next */() => null)
      )

      spec = this[_getRelpathSpec](spec, real)
    }
    return spec
  }

  [_getRelpathSpec] (spec, filepath) {
    /* istanbul ignore else - should also be covered by realpath failure */
    if (filepath) {
      const { name } = spec
      spec = npa(`file:${relpath(this.path, filepath)}`, this.path)
      spec.name = name
    }
    return spec
  }

  // TODO: provide a way to fix bundled deps by exposing metadata about
  // what's in the bundle at each published manifest.  Without that, we
  // can't possibly fix bundled deps without breaking a ton of other stuff,
  // and leaving the user subject to getting it overwritten later anyway.
  [_queueVulnDependents] (options) {
    for (const {nodes} of this.auditReport.values()) {
      for (const node of nodes) {
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
      for (const [name, topVuln] of this.auditReport.topVulns.entries()) {
        const {
          simpleRange,
          topNodes,
          fixAvailable,
        } = topVuln
        for (const node of topNodes) {
          if (node !== this.idealTree) {
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
          options.add.push(`${name}@${version}`)
        }
      }
    }
  }

  [_isVulnerable] (node) {
    return this.auditReport && this.auditReport.isVulnerable(node)
  }

  [_avoidRange] (name) {
    if (!this.auditReport)
      return null
    const vuln = this.auditReport.get(name)
    if (!vuln)
      return null
    return vuln.range
  }

  [_queueNamedUpdates] () {
    // ignore top nodes, since they are not loaded the same way, and
    // probably have their own project associated with them.

    // for every node with one of the names on the list, we add its
    // dependents to the queue to be evaluated.  in buildDepStem,
    // anything on the update names list will get refreshed, even if
    // it isn't a problem.

    // XXX this could be faster by doing a series of inventory.query('name')
    // calls rather than walking over everything in the tree.
    const set = this.idealTree.inventory
      .filter(n => this[_shouldUpdateNode](n))
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
    if (inventory.size === 0 || !(ancient || old && this[_complete]))
      return

    // if the lockfile is from node v5 or earlier, then we'll have to reload
    // all the manifests of everything we encounter.  this is costly, but at
    // least it's just a one-time hit.
    process.emit('time', 'idealTree:inflate')

    const heading = ancient ? 'ancient lockfile' : 'old lockfile'
    this.log.warn(heading,
      `
The ${meta.type} file was created with an old version of npm,
so supplemental metadata must be fetched from the registry.

This is a one-time fix-up, please be patient...
`)

    this.addTracker('idealTree:inflate')
    const queue = []
    for (const node of inventory.values()) {
      queue.push(async () => {
        this.log.silly('inflate', node.location)
        const id = `${node.name}@${node.version}`
        const sloc = node.location.substr('node_modules/'.length)
        const t = `idealTree:inflate:${sloc}`
        this.addTracker(t)
        await pacote.manifest(id, {
          ...this.options,
          resolved: node.resolved,
          integrity: node.integrity,
          fullMetadata: false,
        }).then(mani => {
          node.package = { ...mani, _id: `${mani.name}@${mani.version}` }
        }).catch((er) => {
          const warning = `Could not fetch metadata for ${id}`
          this.log.warn(heading, warning, er)
        })
        this.finishTracker(t)
      })
    }
    await promiseCallLimit(queue)
    this.finishTracker('idealTree:inflate')
    process.emit('timeEnd', 'idealTree:inflate')
  }

  // at this point we have a virtual tree with the actual root node's
  // package deps, which may be partly or entirely incomplete, invalid
  // or extraneous.
  [_buildDeps] (node) {
    process.emit('time', 'idealTree:buildDeps')
    this[_depsQueue].push(this.idealTree)
    this.log.silly('idealTree', 'buildDeps')
    this.addTracker('idealTree', this.idealTree.name, '')
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

    if (!this[_depsQueue].length)
      return this[_resolveLinks]()

    // sort physically shallower deps up to the front of the queue,
    // because they'll affect things deeper in, then alphabetical
    this[_depsQueue].sort((a, b) =>
      (a.depth - b.depth) || a.path.localeCompare(b.path))

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
        hasShrinkwrap && !this[_complete])
      return this[_buildDepStep]()

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
        await pacote.extract(node.resolved, path, opt)

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

    const tasks = await Promise.all(
      // resolve all the edges into nodes using pacote.manifest
      // return a {dep,edge} object so that we can track the reason
      // for this node through the parallelized async operation.
      // note that dep.edgesOut will have all its peer deps resolved,
      // since they're relevant in the calculation about where to place
      // the new and/or updated dependency.
      this[_problemEdges](node).map(edge => this[_nodeFromEdge](edge)
        .then(dep => ({edge, dep})))
    )

    const placed = tasks
      .sort((a, b) => a.edge.name.localeCompare(b.edge.name))
      .map(({ edge, dep }) => this[_placeDep](dep, node, edge))

    const promises = []
    for (const set of placed) {
      for (const node of set) {
        this[_mutateTree] = true
        this.addTracker('idealTree', node.name, node.location)
        this[_depsQueue].push(node)

        // we're certainly going to need these soon, fetch them asap
        // if it fails at this point, though, dont' worry because it
        // may well be an optional dep that has gone missing.  it'll
        // fail later anyway.
        const from = fromPath(node)
        promises.push(...this[_problemEdges](node).map(e =>
          this[_fetchManifest](npa.resolve(e.name, e.spec, from))
            .catch(er => null)))
      }
    }
    await Promise.all(promises)

    return this[_buildDepStep]()
  }

  // loads a node from an edge, and then loads its peer deps (and their
  // peer deps, on down the line) into a virtual root parent.
  [_nodeFromEdge] (edge, parent) {
    // create a virtual root node with the same deps as the node that
    // is requesting this one, so that we can get all the peer deps in
    // a context where they're likely to be resolvable.
    const { legacyPeerDeps } = this
    parent = parent || new Node({
      path: '/virtual-root',
      sourceReference: edge.from,
      legacyPeerDeps,
    })

    const spec = npa.resolve(edge.name, edge.spec, edge.from.path)
    return this[_nodeFromSpec](edge.name, spec, parent, edge)
      .then(node => {
      // handle otherwise unresolvable dependency nesting loops by
      // creating a symbolic link
      // a1 -> b1 -> a2 -> b2 -> a1 -> ...
      // instead of nesting forever, when the loop occurs, create
      // a symbolic link to the earlier instance
        for (let p = edge.from.resolveParent; p; p = p.resolveParent) {
          if (p.matches(node) && !p.isRoot)
            return new Link({ parent, target: p })
        }
        return this[_loadPeerSet](node)
      })
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
    const bd = node.isRoot ? null : node.package.bundleDependencies
    const bundled = new Set(bd || [])

    return [...node.edgesOut.values()]
      .filter(edge => {
        // If it's included in a bundle, we take whatever is specified.
        if (bundled.has(edge.name))
          return false

        // If it's already been logged as a load failure, skip it.
        if (edge.to && this[_loadFailures].has(edge.to))
          return false

        // If it's shrinkwrapped, we use what the shrinkwap wants.
        if (edge.to && edge.to.inShrinkwrap)
          return false

        // If the edge has an error, there's a problem.
        if (!edge.valid)
          return true

        // If the edge has no destination, that's a problem.
        if (!edge.to)
          return edge.type !== 'peerOptional'

        // If user has explicitly asked to update this package by name, it's a problem.
        if (this[_updateNames].includes(edge.name))
          return true

        // If we're fixing a security vulnerability with this package, it's a problem.
        if (this[_isVulnerable](edge.to))
          return true

        // If the user has explicitly asked to install this package, it's a problem.
        if (node.isRoot && this[_explicitRequests].has(edge.name))
          return true

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

    if (this[_manifests].has(spec.raw))
      return this[_manifests].get(spec.raw)
    else {
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
  [_loadPeerSet] (node) {
    const peerEdges = [...node.edgesOut.values()]
      .filter(e => e.peer && !e.valid)
      .map(e => node.parent && node.parent.edgesOut.get(e.name) || e)
    return Promise.all(
      peerEdges.map(edge => this[_nodeFromEdge](edge, node.parent))
    ).then(() => node)
  }

  // starting from either node, or in the case of non-root peer deps,
  // the node's parent, walk up the tree until we find the first spot
  // where this dep cannot be placed, and use the one right before that.
  // place dep, requested by node, to satisfy edge
  [_placeDep] (dep, node, edge, peerEntryEdge = null) {
    if (edge.to &&
        !edge.error &&
        !this[_updateNames].includes(edge.name) &&
        !this[_isVulnerable](edge.to))
      return []

    // top nodes should still get peer deps from their parent or fsParent
    // if possible, and only install locally if there's no other option,
    // eg for a link outside of the project root.
    const start = edge.peer && !node.isRoot
      ? node.resolveParent || node
      : node

    let target
    let canPlace = null
    let warnPeer = false
    for (let check = start; check; check = check.resolveParent) {
      const cp = this[_canPlaceDep](dep, check, edge, peerEntryEdge)

      // anything other than a conflict is fine to proceed with
      if (cp !== CONFLICT) {
        canPlace = cp
        target = check
      } else {
        if (check === start) {
          // if it's a peer dep, and the first place we're putting it conflicts
          // because the node has a direct dependency on the pkg in question,
          // then we treat that as an override when --force is applied, and
          // just warn about it.
          const checkEdge = check.edgesOut.get(edge.name)
          warnPeer = check === start && edge.peer && checkEdge
        }
        break
      }

      // nest packages like npm v1 and v2
      // very disk-inefficient
      if (this[_legacyBundling])
        break

      // when installing globally, or just in global style, we never place
      // deps above the first level.
      if (this[_globalStyle] && check.resolveParent === this.idealTree)
        break
    }

    if (!target) {
      const curNode = node.resolve(edge.name)
      const pc = this[_peerConflict] || { peer: null, current: null }
      // we'll only get one of these
      const current = curNode ? curNode.explain() : pc.current
      const peerConflict = pc.peer
      const expl = {
        code: 'ERESOLVE',
        dep: dep.explain(edge),
        current,
        peerConflict,
        fixWithForce: edge.peer && !!warnPeer,
        type: edge.type,
        isPeer: edge.peer,
      }
      const override = this[_force] || !this[_strictPeerDeps]

      if (override && expl.fixWithForce) {
        this.log.warn('ERESOLVE', 'overriding peer dependency', expl)
        return []
      } else {
        const er = new Error('unable to resolve dependency tree')
        throw Object.assign(er, expl)
      }
    }

    this.log.silly(
      'placeDep',
      target.location || 'ROOT',
      `${edge.name}@${edge.spec}`,
      canPlace,
      `for: ${node.package._id || node.location}`
    )

    // it worked, so we clearly have no peer conflicts at this point.
    this[_peerConflict] = null

    // Can only get KEEP here if the original edge was valid,
    // and we're checking for an update but it's already up to date.
    if (canPlace === KEEP) {
      dep.parent = null
      return []
    }

    // figure out which of this node's peer deps will get placed as well
    const virtualRoot = dep.parent

    const placed = [dep]
    const oldChild = target.children.get(edge.name)
    if (oldChild) {
      // if we're replacing, we should also remove any nodes for edges that
      // are now invalid, and where this (or its deps) is the only dependent,
      // and also recurse on that pruning.  Otherwise leaving that dep node
      // around can result in spurious conflicts pushing nodes deeper into
      // the tree than needed in the case of cycles that will be removed
      // later anyway.
      const oldDeps = []
      for (const [name, edge] of oldChild.edgesOut.entries()) {
        if (!dep.edgesOut.has(name) && edge.to)
          oldDeps.push(edge.to)
      }
      dep.replace(oldChild)
      this[_pruneForReplacement](dep, oldDeps)
      // this may also create some invalid edges, for example if we're
      // intentionally causing something to get nested which was previously
      // placed in this location.
      for (const edge of dep.edgesIn) {
        if (edge.invalid) {
          this[_depsQueue].push(edge.from)
          this[_depsSeen].delete(edge.from)
        }
      }
    } else
      dep.parent = target

    // If the edge is not an error, then we're updating something, and
    // MAY end up putting a better/identical node further up the tree in
    // a way that causes an unnecessary duplication.  If so, remove the
    // now-unnecessary node.
    if (edge.valid && edge.to.parent !== target && dep.canReplace(edge.to))
      edge.to.parent = null

    // visit any dependents who are upset by this change
    for (const edge of dep.edgesIn) {
      if (!edge.valid) {
        this.addTracker('idealTree', edge.from.name, edge.from.location)
        this[_depsQueue].push(edge.from)
      }
    }

    // in case we just made some duplicates that can be removed,
    // prune anything deeper in the tree that can be replaced by this
    if (this.idealTree) {
      for (const node of this.idealTree.inventory.query('name', dep.name)) {
        if (node !== dep &&
            node.isDescendantOf(target) &&
            !node.inShrinkwrap &&
            !node.inBundle &&
            node.canReplaceWith(dep)) {
          // don't prune if the dupe is necessary!
          // root (a, d)
          // +-- a (b, c2)
          // |   +-- b (c2) <-- place c2 for b, lands at root
          // +-- d (e)
          //     +-- e (c1, d)
          //         +-- c1
          //         +-- f (c2)
          //             +-- c2 <-- pruning this would be bad

          const mask = node.parent !== target &&
            node.parent.parent !== target &&
            node.parent.parent.resolve(dep.name)

          if (!mask || mask === dep || node.canReplaceWith(mask))
            node.parent = null
        }
      }
    }

    // also place its unmet or invalid peer deps at this location
    // note that dep has now been removed from the virtualRoot set
    // by virtue of being placed in the target's node_modules.
    if (virtualRoot) {
      const peers = []
      // double loop so that we don't yank things out and then fail to find
      // them in the virtualRoot's children.
      for (const peerEdge of dep.edgesOut.values()) {
        // XXX needs some rework
        if (peerEdge.peer && !peerEdge.valid) {
          const peer = virtualRoot.children.get(peerEdge.name) /* istanbul ignore next - should be impossible */ ||
            peerEdge.to
          /* istanbul ignore else - should be impossible */
          if (peer)
            peers.push([peer, peerEdge])
        }
      }

      for (const [peer, peerEdge] of peers) {
        const peerPlaced = this[_placeDep](
          peer, dep, peerEdge, peerEntryEdge || edge)
        placed.push(...peerPlaced)
      }
    }

    return placed
  }

  [_pruneForReplacement] (node, oldDeps) {
    // gather up all the invalid edgesOut, and any now-extraneous
    // deps that the new node doesn't depend on but the old one did.
    const invalidDeps = new Set([...node.edgesOut.values()]
      .filter(e => e.to && !e.valid).map(e => e.to))
    for (const dep of oldDeps)
      invalidDeps.add(dep)

    // ignore dependency edges from the node being replaced, but
    // otherwise filter the set down to just the set with no
    // dependencies from outside the set, except the node in question.
    const deps = gatherDepSet(invalidDeps, edge =>
      edge.from !== node && edge.to !== node)

    // now just delete whatever's left, because it's junk
    for (const dep of deps)
      dep.parent = null
  }

  // check if we can place DEP in TARGET to satisfy EDGE
  // Need to verify:
  // - no child by that name there already
  // - target does not have a peer dep on name
  // - no higher-level pkg by that name and incompatible spec is depended on
  //   by anything lower in the tree.
  // - node's peer deps and meta-peer deps are siblings in a virtual root at
  //   this point.  make sure that the whole family can come along, so apply
  //   the same checks to each of them.  They may land higher up in the tree,
  //   but we need to know that they CAN live here.
  // Responses:
  // - OK - Yes, because there is nothing there and no conflicts caused
  // - REPLACE - Yes, and you can clobber what's there
  // - KEEP - No, but what's there is fine
  // - CONFLICT - You may not put that there
  //
  // Check peers on OK or REPLACE.  KEEP and CONFLICT do not require peer
  // checking, because either we're leaving it alone, or it won't work anyway.
  // When we check peers, we pass along the peerEntryEdge to track the
  // original edge that caused us to load the family of peer dependencies.
  [_canPlaceDep] (dep, target, edge, peerEntryEdge = null) {
    // peer deps of root deps are effectively root deps
    const isRootDep = target.isRoot && (
      // a direct dependency from the root node
      edge.from === target ||
      // a member of the peer set of a direct root dependency
      peerEntryEdge && peerEntryEdge.from === target
    )

    const entryEdge = peerEntryEdge || edge

    // has child by that name already
    if (target.children.has(dep.name)) {
      const current = target.children.get(dep.name)
      // if the integrities match, then it's literally the same exact bytes,
      // even if it came from somewhere else.
      if (dep.integrity && dep.integrity === current.integrity)
        return KEEP

      // we can always place the root's deps in the root nm folder
      if (isRootDep)
        return this[_canPlacePeers](dep, target, edge, REPLACE, peerEntryEdge)

      // if the version is greater, try to use the new one
      const curVer = current.version
      const newVer = dep.version
      // always try to replace if the version is greater
      const tryReplace = curVer && newVer && semver.gte(newVer, curVer)
      if (tryReplace && current.canReplaceWith(dep))
        return this[_canPlacePeers](dep, target, edge, REPLACE, peerEntryEdge)

      // ok, see if the current one satisfies the edge we're working on then
      if (edge.satisfiedBy(current))
        return KEEP

      // last try, if we prefer deduplication over novelty, check to see if
      // this (older) dep can satisfy the needs of the less nested instance
      if (this[_preferDedupe] && current.canReplaceWith(dep)) {
        const res = this[_canPlacePeers](dep, target, edge, REPLACE, peerEntryEdge)
        /* istanbul ignore else - It's extremely rare that a replaceable
         * node would be a conflict, if the current one wasn't a conflict,
         * but it is theoretically possible if peer deps are pinned.  In
         * that case we treat it like any other conflict, and keep trying */
        if (res !== CONFLICT)
          return res
      }

      // if this is a peer dep, AND target is the resolveParent of the edge,
      // then this is the only place it can go.  If the current node is not
      // a non-peer dependency of this specific target, then it can replace
      // and dupe it deeper in the tree.  If the current node is a peer dep
      // in a set that is a non-peer dep of a deeper target, then replace
      // the whole peer set and the module bringing it in, and add the
      // dependent to the queue for re-evaluation.
      if (edge.peer && target === edge.from.resolveParent && !peerEntryEdge) {
        const peerSet = getPeerSet(current)
        for (const p of peerSet) {
          // if any have a non-peer dep from the target, or a peer dep if
          // the target is root, then we can't safely replace.
          for (const edge of p.edgesIn) {
            if (edge.peer) {
              // root deps take precedence always.
              // in case this is an edge coming from a link it's also
              // going to conflict since deps are effectively relative
              // to its link node parent
              if (edge.from.isTop)
                return CONFLICT

              // other peer deps on this node are irrelevant though.
              continue
            }
            // note that we MAY resolve this conflict by using the target's
            // conflicting dep on the peer, if --force is set.
            if (edge.from === target)
              return CONFLICT
          }
        }
        // all peers could be nested deeper in the tree, so replace
        // adding to the queue will happen later when we scan dep's edgesIn
        return REPLACE
      }

      // no agreement could be reached :(
      return CONFLICT
    }

    // check to see if the target DOESN'T have a child by that name,
    // but DOES have a conflicting dependency of its own.  no need to check
    // if this is the edge we're already looking to resolve!
    if (target !== entryEdge.from && target.edgesOut.has(dep.name)) {
      const edge = target.edgesOut.get(dep.name)
      // It might be that the dep would not be valid here, BUT some other
      // version would.  Could to try to resolve that, but that makes this no
      // longer a pure synchronous function.  ugh.
      // This is a pretty unlikely scenario in a normal install, because we
      // resolve the peer dep set against the parent dependencies, and
      // presumably they all worked together SOMEWHERE to get published in the
      // first place, and since we resolve shallower deps before deeper ones,
      // this can only occur by a child having a peer dep that does not satisfy
      // the parent.  It can happen if we're doing a deep update limited by
      // a specific name, however, or if a dep makes an incompatible change
      // to its peer dep in a non-semver-major version bump, or if the parent
      // is unbounded in its dependency list.
      if (!edge.satisfiedBy(dep))
        return CONFLICT
    }

    // check to see what the name resolves to here, and who depends on it
    // and if they'd be ok with the new dep being there instead.  we know
    // at this point that it's not the target's direct child node.  this is
    // only a check we do when deduping.  if it's a direct dep of the target,
    // then we just make the invalid edge and resolve it later.
    const current = target !== entryEdge.from && target.resolve(dep.name)
    if (current) {
      for (const edge of current.edgesIn.values()) {
        if (edge.from.isDescendantOf(target) && edge.valid) {
          if (!edge.satisfiedBy(dep))
            return CONFLICT
        }
      }
    }

    return this[_canPlacePeers](dep, target, edge, OK, peerEntryEdge)
  }

  // make sure the family of peer deps can live here alongside it.
  // this doesn't guarantee that THIS solution will be the one we take,
  // but it does establish that SOME solution exists at this level in
  // the tree.
  [_canPlacePeers] (dep, target, edge, ret, peerEntryEdge) {
    if (!dep.parent || peerEntryEdge)
      return ret

    for (const peer of dep.parent.children.values()) {
      if (peer !== dep) {
        const peerEdge = dep.edgesOut.get(peer.name) ||
          [...peer.edgesIn].find(e => e.peer)
        /* istanbul ignore else - pretty sure this is impossible, but just
           being cautious */
        if (peerEdge) {
          const canPlacePeer = this[_canPlaceDep](peer, target, peerEdge, edge)
          if (canPlacePeer === CONFLICT) {
            const current = target.resolve(peer.name)
            this[_peerConflict] = {
              peer: peer.explain(peerEdge),
              current: current && current.explain(),
            }
            return CONFLICT
          }
        }
      }
    }

    return ret
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
      const realpath = link.realpath
      const loc = relpath(this.path, realpath)
      const fromInv = this.idealTree.inventory.get(loc)
      if (fromInv && fromInv !== link.target)
        link.target = fromInv

      const external = /^\.\.\//.test(loc)

      if (external && !this[_follow]) {
        // outside the root, somebody else's problem, ignore it
        continue
      }

      if (!link.target.parent && !link.target.fsParent) {
        // the fsParent MUST be some node in the tree, possibly the root.
        // find it by walking up.  Note that this is where its deps may
        // end up being installed, if possible.
        const parts = loc.split('/')
        for (let p = parts.length - 1; p > -1; p--) {
          const path = parts.slice(0, p).join('/')
          if (!path && external)
            break
          const node = !path ? this.idealTree
            : this.idealTree.inventory.get(path)
          if (node) {
            link.target.fsParent = node
            this.addTracker('idealTree', link.target.name, link.target.location)
            this[_depsQueue].push(link.target)
            p = -1
          }
        }
      }

      // didn't find a parent for it, but we're filling in external
      // link targets, so go ahead and process it.
      if (this[_follow] && !link.target.parent && !link.target.fsParent) {
        this.addTracker('idealTree', link.target.name, link.target.location)
        this[_depsQueue].push(link.target)
      }
    }

    if (this[_depsQueue].length)
      return this[_buildDepStep]()
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
    if (metaFromDisk && mutateTree)
      this[_resetDepFlags]()

    // update all the dev/optional/etc flags in the tree
    // either we started with a fresh tree, or we
    // reset all the flags to find the extraneous nodes.
    //
    // if we started from a blank slate, or changed something, then
    // the dep flags will be all set to true.
    if (!metaFromDisk || mutateTree)
      calcDepFlags(this.idealTree)
    else {
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
    if (this[_prune] && needPrune)
      this[_idealTreePrune]()
    process.emit('timeEnd', 'idealTree:fixDepFlags')
  }

  [_idealTreePrune] () {
    for (const node of this.idealTree.inventory.filter(n => n.extraneous))
      node.parent = null
  }

  // we'll need to actually do a walk from the root, because you can have
  // a cycle of deps that all depend on each other, but no path from root.
  // Also, since the ideal tree is loaded from the shrinkwrap, it had
  // extraneous flags set false that might now be actually extraneous, and
  // dev/optional flags that are also now incorrect.  This method sets
  // all flags to true, so we can find the set that is actually extraneous.
  [_resetDepFlags] () {
    for (const node of this.idealTree.inventory.values()) {
      node.extraneous = true
      node.dev = true
      node.devOptional = true
      node.peer = true
      node.optional = true
    }
  }

  [_pruneFailedOptional] () {
    for (const node of this[_loadFailures]) {
      if (!node.optional)
        throw node.errors[0]
      const set = optionalSet(node)
      for (const node of set)
        node.parent = null
    }
  }
}
