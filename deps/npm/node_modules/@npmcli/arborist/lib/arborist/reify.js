// mixin implementing the reify method

const onExit = require('../signal-handling.js')
const pacote = require('pacote')
const rpj = require('read-package-json-fast')
const { updateDepSpec } = require('../dep-spec.js')
const AuditReport = require('../audit-report.js')
const {subset} = require('semver')

const {dirname, resolve, relative} = require('path')
const {depth: dfwalk} = require('treeverse')
const fs = require('fs')
const {promisify} = require('util')
const symlink = promisify(fs.symlink)
const mkdirp = require('mkdirp-infer-owner')
const moveFile = require('@npmcli/move-file')
const rimraf = promisify(require('rimraf'))
const packageContents = require('@npmcli/installed-package-contents')
const { checkEngine, checkPlatform } = require('npm-install-checks')

const treeCheck = require('../tree-check.js')
const relpath = require('../relpath.js')
const Diff = require('../diff.js')
const retirePath = require('../retire-path.js')
const promiseAllRejectLate = require('promise-all-reject-late')
const optionalSet = require('../optional-set.js')
const updateRootPackageJson = require('../update-root-package-json.js')

const _retiredPaths = Symbol('retiredPaths')
const _retiredUnchanged = Symbol('retiredUnchanged')
const _sparseTreeDirs = Symbol('sparseTreeDirs')
const _sparseTreeRoots = Symbol('sparseTreeRoots')
const _savePrefix = Symbol('savePrefix')
const _retireShallowNodes = Symbol.for('retireShallowNodes')
const _getBundlesByDepth = Symbol('getBundlesByDepth')
const _registryResolved = Symbol('registryResolved')
const _addNodeToTrashList = Symbol('addNodeToTrashList')
// shared by rebuild mixin
const _trashList = Symbol.for('trashList')
const _handleOptionalFailure = Symbol.for('handleOptionalFailure')
const _loadTrees = Symbol.for('loadTrees')

// shared symbols for swapping out when testing
const _diffTrees = Symbol.for('diffTrees')
const _createSparseTree = Symbol.for('createSparseTree')
const _loadShrinkwrapsAndUpdateTrees = Symbol.for('loadShrinkwrapsAndUpdateTrees')
const _shrinkwrapUnpacked = Symbol('shrinkwrapUnpacked')
const _reifyNode = Symbol.for('reifyNode')
const _extractOrLink = Symbol('extractOrLink')
// defined by rebuild mixin
const _checkBins = Symbol.for('checkBins')
const _symlink = Symbol('symlink')
const _warnDeprecated = Symbol('warnDeprecated')
const _loadAncientPackageDetails = Symbol('loadAncientPackageDetails')
const _loadBundlesAndUpdateTrees = Symbol.for('loadBundlesAndUpdateTrees')
const _submitQuickAudit = Symbol('submitQuickAudit')
const _awaitQuickAudit = Symbol('awaitQuickAudit')
const _unpackNewModules = Symbol.for('unpackNewModules')
const _moveContents = Symbol.for('moveContents')
const _moveBackRetiredUnchanged = Symbol.for('moveBackRetiredUnchanged')
const _build = Symbol.for('build')
const _removeTrash = Symbol.for('removeTrash')
const _renamePath = Symbol.for('renamePath')
const _rollbackRetireShallowNodes = Symbol.for('rollbackRetireShallowNodes')
const _rollbackCreateSparseTree = Symbol.for('rollbackCreateSparseTree')
const _rollbackMoveBackRetiredUnchanged = Symbol.for('rollbackMoveBackRetiredUnchanged')
const _saveIdealTree = Symbol.for('saveIdealTree')
const _saveLockFile = Symbol('saveLockFile')
const _copyIdealToActual = Symbol('copyIdealToActual')
const _addOmitsToTrashList = Symbol('addOmitsToTrashList')
const _packageLockOnly = Symbol('packageLockOnly')
const _dryRun = Symbol('dryRun')
const _validatePath = Symbol('validatePath')
const _reifyPackages = Symbol('reifyPackages')

const _omitDev = Symbol('omitDev')
const _omitOptional = Symbol('omitOptional')
const _omitPeer = Symbol('omitPeer')

const _global = Symbol.for('global')

// defined by Ideal mixin
const _pruneBundledMetadeps = Symbol.for('pruneBundledMetadeps')
const _explicitRequests = Symbol.for('explicitRequests')
const _resolvedAdd = Symbol.for('resolvedAdd')
const _usePackageLock = Symbol.for('usePackageLock')
const _formatPackageLock = Symbol.for('formatPackageLock')

module.exports = cls => class Reifier extends cls {
  constructor (options) {
    super(options)

    const {
      savePrefix = '^',
      packageLockOnly = false,
      dryRun = false,
      formatPackageLock = true,
    } = options

    this[_dryRun] = !!dryRun
    this[_packageLockOnly] = !!packageLockOnly
    this[_savePrefix] = savePrefix
    this[_formatPackageLock] = !!formatPackageLock

    this.diff = null
    this[_retiredPaths] = {}
    this[_shrinkwrapUnpacked] = new Set()
    this[_retiredUnchanged] = {}
    this[_sparseTreeDirs] = new Set()
    this[_sparseTreeRoots] = new Set()
    this[_trashList] = new Set()
  }

  // public method
  async reify (options = {}) {
    if (this[_packageLockOnly] && this[_global]) {
      const er = new Error('cannot generate lockfile for global packages')
      er.code = 'ESHRINKWRAPGLOBAL'
      throw er
    }

    const omit = new Set(options.omit || [])
    this[_omitDev] = omit.has('dev')
    this[_omitOptional] = omit.has('optional')
    this[_omitPeer] = omit.has('peer')

    // start tracker block
    this.addTracker('reify')
    process.emit('time', 'reify')
    await this[_validatePath]()
      .then(() => this[_loadTrees](options))
      .then(() => this[_diffTrees]())
      .then(() => this[_reifyPackages]())
      .then(() => this[_saveIdealTree](options))
      .then(() => this[_copyIdealToActual]())
      .then(() => this[_awaitQuickAudit]())

    this.finishTracker('reify')
    process.emit('timeEnd', 'reify')
    return treeCheck(this.actualTree)
  }

  async [_validatePath] () {
    // don't create missing dirs on dry runs
    if (this[_packageLockOnly] || this[_dryRun])
      return

    await mkdirp(resolve(this.path))
  }

  async [_reifyPackages] () {
    // we don't submit the audit report or write to disk on dry runs
    if (this[_dryRun])
      return

    if (this[_packageLockOnly]) {
      // we already have the complete tree, so just audit it now,
      // and that's all we have to do here.
      return this[_submitQuickAudit]()
    }

    // ok, we're about to start touching the fs.  need to roll back
    // if we get an early termination.
    let reifyTerminated = null
    const removeHandler = onExit(({signal}) => {
      // only call once.  if signal hits twice, we just terminate
      removeHandler()
      reifyTerminated = Object.assign(new Error('process terminated'), {
        signal,
      })
      return false
    })

    // [rollbackfn, [...actions]]
    // after each step, if the process was terminated, execute the rollback
    // note that each rollback *also* calls the previous one when it's
    // finished, and then the first one throws the error, so we only need
    // a new rollback step when we have a new thing that must be done to
    // revert the install.
    const steps = [
      [_rollbackRetireShallowNodes, [
        _retireShallowNodes,
      ]],
      [_rollbackCreateSparseTree, [
        _createSparseTree,
        _addOmitsToTrashList,
        _loadShrinkwrapsAndUpdateTrees,
        _loadBundlesAndUpdateTrees,
        _submitQuickAudit,
        _unpackNewModules,
      ]],
      [_rollbackMoveBackRetiredUnchanged, [
        _moveBackRetiredUnchanged,
        _build,
      ]],
    ]
    for (const [rollback, actions] of steps) {
      for (const action of actions) {
        try {
          await this[action]()
          if (reifyTerminated)
            throw reifyTerminated
        } catch (er) {
          await this[rollback](er)
          /* istanbul ignore next - rollback throws, should never hit this */
          throw er
        }
      }
    }

    // no rollback for this one, just exit with the error, since the
    // install completed and can't be safely recovered at this point.
    await this[_removeTrash]()
    if (reifyTerminated)
      throw reifyTerminated

    // done modifying the file system, no need to keep listening for sigs
    removeHandler()
  }

  // when doing a local install, we load everything and figure it all out.
  // when doing a global install, we *only* care about the explicit requests.
  [_loadTrees] (options) {
    process.emit('time', 'reify:loadTrees')
    const bitOpt = {
      ...options,
      complete: this[_packageLockOnly] || this[_dryRun],
    }

    // if we're only writing a package lock, then it doesn't matter what's here
    if (this[_packageLockOnly]) {
      return this.buildIdealTree(bitOpt)
        .then(() => process.emit('timeEnd', 'reify:loadTrees'))
    }

    const actualOpt = this[_global] ? {
      ignoreMissing: true,
      global: true,
      filter: (node, kid) =>
        this[_explicitRequests].size === 0 || !node.isProjectRoot ? true
        : (this.idealTree.edgesOut.has(kid) || this[_explicitRequests].has(kid)),
    } : { ignoreMissing: true }

    if (!this[_global]) {
      return Promise.all([this.loadActual(actualOpt), this.buildIdealTree(bitOpt)])
        .then(() => process.emit('timeEnd', 'reify:loadTrees'))
    }

    // the global install space tends to have a lot of stuff in it.  don't
    // load all of it, just what we care about.  we won't be saving a
    // hidden lockfile in there anyway.  Note that we have to load ideal
    // BEFORE loading actual, so that the actualOpt can use the
    // explicitRequests which is set during buildIdealTree
    return this.buildIdealTree(bitOpt)
      .then(() => this.loadActual(actualOpt))
      .then(() => process.emit('timeEnd', 'reify:loadTrees'))
  }

  [_diffTrees] () {
    if (this[_packageLockOnly])
      return

    process.emit('time', 'reify:diffTrees')
    // XXX if we have an existing diff already, there should be a way
    // to just invalidate the parts that changed, but avoid walking the
    // whole tree again.

    // find all the nodes that need to change between the actual
    // and ideal trees.
    this.diff = Diff.calculate({
      actual: this.actualTree,
      ideal: this.idealTree,
    })

    for (const node of this.diff.removed) {
      // a node in a dep bundle will only be removed if its bundling dep
      // is removed as well.  in which case, we don't have to delete it!
      if (!node.inDepBundle)
        this[_addNodeToTrashList](node)
    }
    process.emit('timeEnd', 'reify:diffTrees')
  }

  // add the node and all its bins to the list of things to be
  // removed later on in the process.  optionally, also mark them
  // as a retired paths, so that we move them out of the way and
  // replace them when rolling back on failure.
  [_addNodeToTrashList] (node, retire) {
    const paths = [node.path, ...node.binPaths]
    const moves = this[_retiredPaths]
    this.log.silly('reify', 'mark', retire ? 'retired' : 'deleted', paths)
    for (const path of paths) {
      if (retire) {
        const retired = retirePath(path)
        moves[path] = retired
        this[_trashList].add(retired)
      } else
        this[_trashList].add(path)
    }
  }

  // move aside the shallowest nodes in the tree that have to be
  // changed or removed, so that we can rollback if necessary.
  [_retireShallowNodes] () {
    process.emit('time', 'reify:retireShallow')
    const moves = this[_retiredPaths] = {}
    for (const diff of this.diff.children) {
      if (diff.action === 'CHANGE' || diff.action === 'REMOVE') {
        // we'll have to clean these up at the end, so add them to the list
        this[_addNodeToTrashList](diff.actual, true)
      }
    }
    this.log.silly('reify', 'moves', moves)
    const movePromises = Object.entries(moves)
      .map(([from, to]) => this[_renamePath](from, to))
    return promiseAllRejectLate(movePromises)
      .then(() => process.emit('timeEnd', 'reify:retireShallow'))
  }

  [_renamePath] (from, to, didMkdirp = false) {
    return moveFile(from, to)
      .catch(er => {
        // Occasionally an expected bin file might not exist in the package,
        // or a shim/symlink might have been moved aside.  If we've already
        // handled the most common cause of ENOENT (dir doesn't exist yet),
        // then just ignore any ENOENT.
        if (er.code === 'ENOENT') {
          return didMkdirp ? null : mkdirp(dirname(to)).then(() =>
            this[_renamePath](from, to, true))
        } else if (er.code === 'EEXIST')
          return rimraf(to).then(() => moveFile(from, to))
        else
          throw er
      })
  }

  [_rollbackRetireShallowNodes] (er) {
    process.emit('time', 'reify:rollback:retireShallow')
    const moves = this[_retiredPaths]
    const movePromises = Object.entries(moves)
      .map(([from, to]) => this[_renamePath](to, from))
    return promiseAllRejectLate(movePromises)
      // ignore subsequent rollback errors
      .catch(er => {})
      .then(() => process.emit('timeEnd', 'reify:rollback:retireShallow'))
      .then(() => {
        throw er
      })
  }

  // adding to the trash list will skip reifying, and delete them
  // if they are currently in the tree and otherwise untouched.
  [_addOmitsToTrashList] () {
    if (!this[_omitDev] && !this[_omitOptional] && !this[_omitPeer])
      return

    process.emit('time', 'reify:trashOmits')
    const filter = node =>
      node.peer && this[_omitPeer] ||
      node.dev && this[_omitDev] ||
      node.optional && this[_omitOptional] ||
      node.devOptional && this[_omitOptional] && this[_omitDev]

    for (const node of this.idealTree.inventory.filter(filter))
      this[_addNodeToTrashList](node)

    process.emit('timeEnd', 'reify:trashOmits')
  }

  [_createSparseTree] () {
    process.emit('time', 'reify:createSparse')
    // if we call this fn again, we look for the previous list
    // so that we can avoid making the same directory multiple times
    const dirs = this.diff.leaves
      .filter(diff => {
        return (diff.action === 'ADD' || diff.action === 'CHANGE') &&
          !this[_sparseTreeDirs].has(diff.ideal.path)
      })
      .map(diff => diff.ideal.path)

    return promiseAllRejectLate(dirs.map(d => mkdirp(d)))
      .then(made => {
        made.forEach(made => this[_sparseTreeRoots].add(made))
        dirs.forEach(dir => this[_sparseTreeDirs].add(dir))
      })
      .then(() => process.emit('timeEnd', 'reify:createSparse'))
  }

  [_rollbackCreateSparseTree] (er) {
    process.emit('time', 'reify:rollback:createSparse')
    // cut the roots of the sparse tree that were created, not the leaves
    const roots = this[_sparseTreeRoots]
    // also delete the moves that we retired, so that we can move them back
    const failures = []
    const targets = [...roots, ...Object.keys(this[_retiredPaths])]
    const unlinks = targets
      .map(path => rimraf(path).catch(er => failures.push([path, er])))
    return promiseAllRejectLate(unlinks)
      .then(() => {
        if (failures.length)
          this.log.warn('cleanup', 'Failed to remove some directories', failures)
      })
      .then(() => process.emit('timeEnd', 'reify:rollback:createSparse'))
      .then(() => this[_rollbackRetireShallowNodes](er))
  }

  // shrinkwrap nodes define their dependency branches with a file, so
  // we need to unpack them, read that shrinkwrap file, and then update
  // the tree by calling loadVirtual with the node as the root.
  [_loadShrinkwrapsAndUpdateTrees] () {
    const seen = this[_shrinkwrapUnpacked]
    const shrinkwraps = this.diff.leaves
      .filter(d => (d.action === 'CHANGE' || d.action === 'ADD') &&
        d.ideal.hasShrinkwrap && !seen.has(d.ideal) &&
        !this[_trashList].has(d.ideal.path))

    if (!shrinkwraps.length)
      return

    process.emit('time', 'reify:loadShrinkwraps')

    const Arborist = this.constructor
    return promiseAllRejectLate(shrinkwraps.map(diff => {
      const node = diff.ideal
      seen.add(node)
      return this[_reifyNode](node)
    }))
      .then(nodes => promiseAllRejectLate(nodes.map(node => new Arborist({
        ...this.options,
        path: node.path,
      }).loadVirtual({ root: node }))))
      // reload the diff and sparse tree because the ideal tree changed
      .then(() => this[_diffTrees]())
      .then(() => this[_createSparseTree]())
      .then(() => this[_addOmitsToTrashList]())
      .then(() => this[_loadShrinkwrapsAndUpdateTrees]())
      .then(() => process.emit('timeEnd', 'reify:loadShrinkwraps'))
  }

  // create a symlink for Links, extract for Nodes
  // return the node object, since we usually want that
  // handle optional dep failures here
  // If node is in trash list, skip it
  // If reifying fails, and the node is optional, add it and its optionalSet
  // to the trash list
  // Always return the node.
  [_reifyNode] (node) {
    if (this[_trashList].has(node.path))
      return node

    const timer = `reifyNode:${node.location}`
    process.emit('time', timer)
    this.addTracker('reify', node.name, node.location)

    const { npmVersion, nodeVersion } = this.options
    const p = Promise.resolve()
      .then(() => {
        // when we reify an optional node, check the engine and platform
        // first. be sure to ignore the --force and --engine-strict flags,
        // since we always want to skip any optional packages we can't install.
        // these checks throwing will result in a rollback and removal
        // of the mismatches
        if (node.optional) {
          checkEngine(node.package, npmVersion, nodeVersion, false)
          checkPlatform(node.package, false)
        }
      })
      .then(() => this[_checkBins](node))
      .then(() => this[_extractOrLink](node))
      .then(() => this[_warnDeprecated](node))
      .then(() => this[_loadAncientPackageDetails](node))

    return this[_handleOptionalFailure](node, p)
      .then(() => {
        this.finishTracker('reify', node.name, node.location)
        process.emit('timeEnd', timer)
        return node
      })
  }

  [_extractOrLink] (node) {
    // in normal cases, node.resolved should *always* be set by now.
    // however, it is possible when a lockfile is damaged, or very old,
    // or in some other race condition bugs in npm v6, that a previously
    // bundled dependency will have just a version, but no resolved value,
    // and no 'bundled: true' setting.
    // Do the best with what we have, or else remove it from the tree
    // entirely, since we can't possibly reify it.
    const res = node.resolved ? `${node.name}@${this[_registryResolved](node.resolved)}`
      : node.package.name && node.version
        ? `${node.package.name}@${node.version}`
        : null

    // no idea what this thing is.  remove it from the tree.
    if (!res) {
      const warning = 'invalid or damaged lockfile detected\n' +
        'please re-try this operation once it completes\n' +
        'so that the damage can be corrected, or perform\n' +
        'a fresh install with no lockfile if the problem persists.'
      this.log.warn('reify', warning)
      this.log.verbose('reify', 'unrecognized node in tree', node.path)
      node.parent = null
      node.fsParent = null
      this[_addNodeToTrashList](node)
      return
    }

    return node.isLink
      ? rimraf(node.path).then(() => this[_symlink](node))
      : pacote.extract(res, node.path, {
        ...this.options,
        resolved: node.resolved,
        integrity: node.integrity,
      })
  }

  [_symlink] (node) {
    const dir = dirname(node.path)
    const target = node.realpath
    const rel = relative(dir, target)
    return symlink(rel, node.path, 'junction')
  }

  [_warnDeprecated] (node) {
    const {_id, deprecated} = node.package
    if (deprecated)
      this.log.warn('deprecated', `${_id}: ${deprecated}`)
  }

  async [_loadAncientPackageDetails] (node, forceReload = false) {
    // If we're loading from a v1 lockfile, load details from the package.json
    // that weren't recorded in the old format.
    const {meta} = this.idealTree
    const ancient = meta.ancientLockfile
    const old = meta.loadedFromDisk && !(meta.originalLockfileVersion >= 2)

    // already replaced with the manifest if it's truly ancient
    if (node.path && (forceReload || (old && !ancient))) {
      // XXX should have a shared location where package.json is read,
      // so we don't ever read the same pj more than necessary.
      let pkg
      try {
        pkg = await rpj(node.path + '/package.json')
      } catch (err) {}

      if (pkg) {
        node.package.bin = pkg.bin
        node.package.os = pkg.os
        node.package.cpu = pkg.cpu
        node.package.engines = pkg.engines
        meta.add(node)
      }
    }
  }

  // if the node is optional, then the failure of the promise is nonfatal
  // just add it and its optional set to the trash list.
  [_handleOptionalFailure] (node, p) {
    return (node.optional ? p.catch(er => {
      const set = optionalSet(node)
      for (node of set) {
        this.log.verbose('reify', 'failed optional dependency', node.path)
        this[_addNodeToTrashList](node)
      }
    }) : p).then(() => node)
  }

  [_registryResolved] (resolved) {
    // the default registry url is a magic value meaning "the currently
    // configured registry".
    //
    // XXX: use a magic string that isn't also a valid value, like
    // ${REGISTRY} or something.  This has to be threaded through the
    // Shrinkwrap and Node classes carefully, so for now, just treat
    // the default reg as the magical animal that it has been.
    return resolved && resolved
      .replace(/^https?:\/\/registry.npmjs.org\//, this.registry)
  }

  // bundles are *sort of* like shrinkwraps, in that the branch is defined
  // by the contents of the package.  however, in their case, rather than
  // shipping a virtual tree that must be reified, they ship an entire
  // reified actual tree that must be unpacked and not modified.
  [_loadBundlesAndUpdateTrees] (
    depth = 0, bundlesByDepth = this[_getBundlesByDepth]()
  ) {
    if (depth === 0)
      process.emit('time', 'reify:loadBundles')
    const maxBundleDepth = bundlesByDepth.get('maxBundleDepth')
    if (depth > maxBundleDepth) {
      // if we did something, then prune the tree and update the diffs
      if (maxBundleDepth !== -1) {
        this[_pruneBundledMetadeps](bundlesByDepth)
        this[_diffTrees]()
      }
      process.emit('timeEnd', 'reify:loadBundles')
      return
    }

    // skip any that have since been removed from the tree, eg by a
    // shallower bundle overwriting them with a bundled meta-dep.
    const set = (bundlesByDepth.get(depth) || [])
      .filter(node => node.root === this.idealTree &&
        !this[_trashList].has(node.path))

    if (!set.length)
      return this[_loadBundlesAndUpdateTrees](depth + 1, bundlesByDepth)

    // extract all the nodes with bundles
    return promiseAllRejectLate(set.map(node => this[_reifyNode](node)))
    // then load their unpacked children and move into the ideal tree
      .then(nodes =>
        promiseAllRejectLate(nodes.map(node => new this.constructor({
          ...this.options,
          path: node.path,
        }).loadActual({
          root: node,
          // don't transplant any sparse folders we created
          transplantFilter: node => node.package._id,
        }))))
    // move onto the next level of bundled items
      .then(() => this[_loadBundlesAndUpdateTrees](depth + 1, bundlesByDepth))
  }

  [_getBundlesByDepth] () {
    const bundlesByDepth = new Map()
    let maxBundleDepth = -1
    dfwalk({
      tree: this.diff,
      visit: diff => {
        const node = diff.ideal
        if (node && !node.isProjectRoot && node.package.bundleDependencies &&
            node.package.bundleDependencies.length) {
          maxBundleDepth = Math.max(maxBundleDepth, node.depth)
          if (!bundlesByDepth.has(node.depth))
            bundlesByDepth.set(node.depth, [node])
          else
            bundlesByDepth.get(node.depth).push(node)
        }
      },
      getChildren: diff => diff.children,
    })

    bundlesByDepth.set('maxBundleDepth', maxBundleDepth)
    return bundlesByDepth
  }

  // https://github.com/npm/cli/issues/1597#issuecomment-667639545
  [_pruneBundledMetadeps] (bundlesByDepth) {
    const bundleShadowed = new Set()
    // create the list of nodes shadowed by children of bundlers
    for (const bundles of bundlesByDepth.values()) {
      // skip the 'maxBundleDepth' item
      if (!Array.isArray(bundles))
        continue
      for (const node of bundles) {
        for (const name of node.children.keys()) {
          const shadow = node.parent.resolve(name)
          if (!shadow)
            continue
          bundleShadowed.add(shadow)
          shadow.extraneous = true
        }
      }
    }
    let changed = true
    while (changed) {
      changed = false
      for (const shadow of bundleShadowed) {
        if (!shadow.extraneous) {
          bundleShadowed.delete(shadow)
          continue
        }

        for (const edge of shadow.edgesIn) {
          if (!edge.from.extraneous) {
            shadow.extraneous = false
            bundleShadowed.delete(shadow)
            changed = true
          } else {
            for (const shadDep of shadow.edgesOut.values()) {
              /* istanbul ignore else - pretty unusual situation, just being
               * defensive here. Would mean that a bundled dep has a dependency
               * that is unmet. which, weird, but if you bundle it, we take
               * whatever you put there and assume the publisher knows best. */
              if (shadDep.to)
                bundleShadowed.add(shadDep.to)
            }
          }
        }
      }
    }
    for (const shadow of bundleShadowed) {
      shadow.parent = null
      this[_addNodeToTrashList](shadow)
    }
  }

  [_submitQuickAudit] () {
    if (this.options.audit === false)
      return this.auditReport = null

    // we submit the quick audit at this point in the process, as soon as
    // we have all the deps resolved, so that it can overlap with the other
    // actions as much as possible.  Stash the promise, which we resolve
    // before finishing the reify() and returning the tree.  Thus, we do
    // NOT return the promise, as the intent is for this to run in parallel
    // with the reification, and be resolved at a later time.
    process.emit('time', 'reify:audit')

    this.auditReport = AuditReport.load(this.idealTree, this.options)
      .then(res => {
        process.emit('timeEnd', 'reify:audit')
        this.auditReport = res
      })
  }

  // return the promise if we're waiting for it, or the replaced result
  [_awaitQuickAudit] () {
    return this.auditReport
  }

  // ok!  actually unpack stuff into their target locations!
  // The sparse tree has already been created, so we walk the diff
  // kicking off each unpack job.  If any fail, we rimraf the sparse
  // tree entirely and try to put everything back where it was.
  [_unpackNewModules] () {
    process.emit('time', 'reify:unpack')
    const unpacks = []
    dfwalk({
      tree: this.diff,
      visit: diff => {
        // no unpacking if we don't want to change this thing
        if (diff.action !== 'CHANGE' && diff.action !== 'ADD')
          return

        const node = diff.ideal
        const bd = node.package.bundleDependencies
        const sw = this[_shrinkwrapUnpacked].has(node)

        // check whether we still need to unpack this one.
        // test the inDepBundle last, since that's potentially a tree walk.
        const doUnpack = node && // can't unpack if removed!
          !node.isRoot && // root node already exists
          !(bd && bd.length) && // already unpacked to read bundle
          !sw && // already unpacked to read sw
          !node.inDepBundle // already unpacked by another dep's bundle

        if (doUnpack)
          unpacks.push(this[_reifyNode](node))
      },
      getChildren: diff => diff.children,
    })
    return promiseAllRejectLate(unpacks)
      .then(() => process.emit('timeEnd', 'reify:unpack'))
  }

  // This is the part where we move back the unchanging nodes that were
  // the children of a node that did change.  If this fails, the rollback
  // is a three-step process.  First, we try to move the retired unchanged
  // nodes BACK to their retirement folders, then delete the sparse tree,
  // then move everything out of retirement.
  [_moveBackRetiredUnchanged] () {
    // get a list of all unchanging children of any shallow retired nodes
    // if they are not the ancestor of any node in the diff set, then the
    // directory won't already exist, so just rename it over.
    // This is sort of an inverse diff tree, of all the nodes where
    // the actualTree and idealTree _don't_ differ, starting from the
    // shallowest nodes that we moved aside in the first place.
    process.emit('time', 'reify:unretire')
    const moves = this[_retiredPaths]
    this[_retiredUnchanged] = {}
    return promiseAllRejectLate(this.diff.children.map(diff => {
      const realFolder = (diff.actual || diff.ideal).path
      const retireFolder = moves[realFolder]
      this[_retiredUnchanged][retireFolder] = []
      return promiseAllRejectLate(diff.unchanged.map(node => {
        // no need to roll back links, since we'll just delete them anyway
        if (node.isLink)
          return mkdirp(dirname(node.path)).then(() => this[_reifyNode](node))

        // will have been moved/unpacked along with bundler
        if (node.inDepBundle)
          return

        this[_retiredUnchanged][retireFolder].push(node)

        const rel = relative(realFolder, node.path)
        const fromPath = resolve(retireFolder, rel)
        // if it has bundleDependencies, then make node_modules.  otherwise
        // skip it.
        const bd = node.package.bundleDependencies
        const dir = bd && bd.length ? node.path + '/node_modules' : node.path
        return mkdirp(dir).then(() => this[_moveContents](node, fromPath))
      }))
    }))
      .then(() => process.emit('timeEnd', 'reify:unretire'))
  }

  // move the contents from the fromPath to the node.path
  [_moveContents] (node, fromPath) {
    return packageContents({
      path: fromPath,
      depth: 1,
      packageJsonCache: new Map([[fromPath + '/package.json', node.package]]),
    }).then(res => promiseAllRejectLate(res.map(path => {
      const rel = relative(fromPath, path)
      const to = resolve(node.path, rel)
      return this[_renamePath](path, to)
    })))
  }

  [_rollbackMoveBackRetiredUnchanged] (er) {
    const moves = this[_retiredPaths]
    // flip the mapping around to go back
    const realFolders = new Map(Object.entries(moves).map(([k, v]) => [v, k]))
    const promises = Object.entries(this[_retiredUnchanged])
      .map(([retireFolder, nodes]) => promiseAllRejectLate(nodes.map(node => {
        const realFolder = realFolders.get(retireFolder)
        const rel = relative(realFolder, node.path)
        const fromPath = resolve(retireFolder, rel)
        return this[_moveContents]({ ...node, path: fromPath }, node.path)
      })))
    return promiseAllRejectLate(promises)
      .then(() => this[_rollbackCreateSparseTree](er))
  }

  [_build] () {
    process.emit('time', 'reify:build')

    // for all the things being installed, run their appropriate scripts
    // run in tip->root order, so as to be more likely to build a node's
    // deps before attempting to build it itself
    const nodes = []
    dfwalk({
      tree: this.diff,
      leave: diff => {
        if (!diff.ideal.isProjectRoot)
          nodes.push(diff.ideal)
      },
      // process adds before changes, ignore removals
      getChildren: diff => diff && diff.children,
      filter: diff => diff.action === 'ADD' || diff.action === 'CHANGE',
    })

    return this.rebuild({ nodes, handleOptionalFailure: true })
      .then(() => process.emit('timeEnd', 'reify:build'))
  }

  // the tree is pretty much built now, so it's cleanup time.
  // remove the retired folders, and any deleted nodes
  // If this fails, there isn't much we can do but tell the user about it.
  // Thankfully, it's pretty unlikely that it'll fail, since rimraf is a tank.
  [_removeTrash] () {
    process.emit('time', 'reify:trash')
    const promises = []
    const failures = []
    const rm = path => rimraf(path).catch(er => failures.push([path, er]))

    for (const path of this[_trashList])
      promises.push(rm(path))

    return promiseAllRejectLate(promises).then(() => {
      if (failures.length)
        this.log.warn('cleanup', 'Failed to remove some directories', failures)
    })
      .then(() => process.emit('timeEnd', 'reify:trash'))
  }

  // last but not least, we save the ideal tree metadata to the package-lock
  // or shrinkwrap file, and any additions or removals to package.json
  [_saveIdealTree] (options) {
    // the ideal tree is actualized now, hooray!
    // it still contains all the references to optional nodes that were removed
    // for install failures.  Those still end up in the shrinkwrap, so we
    // save it first, then prune out the optional trash, and then return it.

    // support save=false option
    if (options.save === false || this[_global] || this[_dryRun])
      return

    process.emit('time', 'reify:save')

    if (this[_resolvedAdd]) {
      const root = this.idealTree
      const pkg = root.package
      for (const req of this[_resolvedAdd]) {
        const {name, rawSpec, subSpec} = req
        const spec = subSpec ? subSpec.rawSpec : rawSpec
        const child = root.children.get(name)

        if (req.registry) {
          const version = child.version
          const prefixRange = version ? this[_savePrefix] + version : '*'
          // if we installed a range, then we save the range specified
          // if it is not a subset of the ^x.y.z.  eg, installing a range
          // of `1.x <1.2.3` will not be saved as `^1.2.0`, because that
          // would allow versions outside the requested range.  Tags and
          // specific versions save with the save-prefix.
          const isRange = (subSpec || req).type === 'range'
          const range = !isRange || subset(prefixRange, spec, { loose: true })
            ? prefixRange : spec
          const pname = child.package.name
          const alias = name !== pname
          updateDepSpec(pkg, name, (alias ? `npm:${pname}@` : '') + range)
        } else if (req.hosted) {
          // save the git+https url if it has auth, otherwise shortcut
          const h = req.hosted
          const opt = { noCommittish: false }
          const save = h.https && h.auth ? `git+${h.https(opt)}`
            : h.shortcut(opt)
          updateDepSpec(pkg, name, save)
        } else
          updateDepSpec(pkg, name, req.saveSpec)
      }

      // refresh the edges so they have the correct specs
      this.idealTree.package = pkg
    }

    // preserve indentation, if possible
    const {
      [Symbol.for('indent')]: indent,
    } = this.idealTree.package
    const format = indent === undefined ? '  ' : indent

    const saveOpt = {
      format: (this[_formatPackageLock] && format) ? format
      : this[_formatPackageLock],
    }

    return Promise.all([
      this[_saveLockFile](saveOpt),
      updateRootPackageJson(this.idealTree),
    ]).then(() => process.emit('timeEnd', 'reify:save'))
  }

  async [_saveLockFile] (saveOpt) {
    if (!this[_usePackageLock])
      return

    const { meta } = this.idealTree

    // might have to update metadata for bins and stuff that gets lost
    if (meta.loadedFromDisk && !(meta.originalLockfileVersion >= 2)) {
      for (const node of this.idealTree.inventory.values())
        await this[_loadAncientPackageDetails](node, true)
    }

    return meta.save(saveOpt)
  }

  [_copyIdealToActual] () {
    // save the ideal's meta as a hidden lockfile after we actualize it
    this.idealTree.meta.filename =
      this.path + '/node_modules/.package-lock.json'
    this.idealTree.meta.hiddenLockfile = true
    this.actualTree = this.idealTree
    this.idealTree = null
    for (const path of this[_trashList]) {
      const loc = relpath(this.path, path)
      const node = this.actualTree.inventory.get(loc)
      if (node && node.root === this.actualTree)
        node.parent = null
    }

    return !this[_global] && this.actualTree.meta.save()
  }
}
