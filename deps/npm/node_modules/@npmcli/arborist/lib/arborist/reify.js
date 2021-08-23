// mixin implementing the reify method

const onExit = require('../signal-handling.js')
const pacote = require('pacote')
const AuditReport = require('../audit-report.js')
const {subset, intersects} = require('semver')
const npa = require('npm-package-arg')
const debug = require('../debug.js')
const walkUp = require('walk-up-path')

const {dirname, resolve, relative} = require('path')
const {depth: dfwalk} = require('treeverse')
const fs = require('fs')
const {promisify} = require('util')
const lstat = promisify(fs.lstat)
const symlink = promisify(fs.symlink)
const mkdirp = require('mkdirp-infer-owner')
const justMkdirp = require('mkdirp')
const moveFile = require('@npmcli/move-file')
const rimraf = promisify(require('rimraf'))
const PackageJson = require('@npmcli/package-json')
const packageContents = require('@npmcli/installed-package-contents')
const { checkEngine, checkPlatform } = require('npm-install-checks')
const _force = Symbol.for('force')

const treeCheck = require('../tree-check.js')
const relpath = require('../relpath.js')
const Diff = require('../diff.js')
const retirePath = require('../retire-path.js')
const promiseAllRejectLate = require('promise-all-reject-late')
const optionalSet = require('../optional-set.js')
const calcDepFlags = require('../calc-dep-flags.js')
const { saveTypeMap, hasSubKey } = require('../add-rm-pkg-deps.js')

const _retiredPaths = Symbol('retiredPaths')
const _retiredUnchanged = Symbol('retiredUnchanged')
const _sparseTreeDirs = Symbol('sparseTreeDirs')
const _sparseTreeRoots = Symbol('sparseTreeRoots')
const _savePrefix = Symbol('savePrefix')
const _retireShallowNodes = Symbol.for('retireShallowNodes')
const _getBundlesByDepth = Symbol('getBundlesByDepth')
const _registryResolved = Symbol('registryResolved')
const _addNodeToTrashList = Symbol('addNodeToTrashList')
const _workspaces = Symbol.for('workspaces')

// shared by rebuild mixin
const _trashList = Symbol.for('trashList')
const _handleOptionalFailure = Symbol.for('handleOptionalFailure')
const _loadTrees = Symbol.for('loadTrees')

// shared symbols for swapping out when testing
const _diffTrees = Symbol.for('diffTrees')
const _createSparseTree = Symbol.for('createSparseTree')
const _loadShrinkwrapsAndUpdateTrees = Symbol.for('loadShrinkwrapsAndUpdateTrees')
const _shrinkwrapInflated = Symbol('shrinkwrapInflated')
const _bundleUnpacked = Symbol('bundleUnpacked')
const _bundleMissing = Symbol('bundleMissing')
const _reifyNode = Symbol.for('reifyNode')
const _extractOrLink = Symbol('extractOrLink')
// defined by rebuild mixin
const _checkBins = Symbol.for('checkBins')
const _symlink = Symbol('symlink')
const _warnDeprecated = Symbol('warnDeprecated')
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
const _validateNodeModules = Symbol('validateNodeModules')
const _nmValidated = Symbol('nmValidated')
const _validatePath = Symbol('validatePath')
const _reifyPackages = Symbol.for('reifyPackages')

const _omitDev = Symbol('omitDev')
const _omitOptional = Symbol('omitOptional')
const _omitPeer = Symbol('omitPeer')

const _global = Symbol.for('global')

const _pruneBundledMetadeps = Symbol('pruneBundledMetadeps')

// defined by Ideal mixin
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
    this[_shrinkwrapInflated] = new Set()
    this[_retiredUnchanged] = {}
    this[_sparseTreeDirs] = new Set()
    this[_sparseTreeRoots] = new Set()
    this[_trashList] = new Set()
    // the nodes we unpack to read their bundles
    this[_bundleUnpacked] = new Set()
    // child nodes we'd EXPECT to be included in a bundle, but aren't
    this[_bundleMissing] = new Set()
    this[_nmValidated] = new Set()
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
    await this[_loadTrees](options)
    await this[_diffTrees]()
    await this[_reifyPackages]()
    await this[_saveIdealTree](options)
    await this[_copyIdealToActual]()
    await this[_awaitQuickAudit]()

    this.finishTracker('reify')
    process.emit('timeEnd', 'reify')
    return treeCheck(this.actualTree)
  }

  async [_validatePath] () {
    // don't create missing dirs on dry runs
    if (this[_packageLockOnly] || this[_dryRun])
      return

    // we do NOT want to set ownership on this folder, especially
    // recursively, because it can have other side effects to do that
    // in a project directory.  We just want to make it if it's missing.
    await justMkdirp(resolve(this.path))

    // do not allow the top-level node_modules to be a symlink
    await this[_validateNodeModules](resolve(this.path, 'node_modules'))
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
      filter: (node, kid) => {
        // if it's not the project root, and we have no explicit requests,
        // then we're already into a nested dep, so we keep it
        if (this.explicitRequests.size === 0 || !node.isProjectRoot)
          return true

        // if we added it as an edgeOut, then we want it
        if (this.idealTree.edgesOut.has(kid))
          return true

        // if it's an explicit request, then we want it
        const hasExplicit = [...this.explicitRequests]
          .some(edge => edge.name === kid)
        if (hasExplicit)
          return true

        // ignore the rest of the global install folder
        return false
      },
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

    const filterNodes = []
    if (this[_global] && this.explicitRequests.size) {
      const idealTree = this.idealTree.target
      const actualTree = this.actualTree.target
      // we ONLY are allowed to make changes in the global top-level
      // children where there's an explicit request.
      for (const { name } of this.explicitRequests) {
        const ideal = idealTree.children.get(name)
        if (ideal)
          filterNodes.push(ideal)
        const actual = actualTree.children.get(name)
        if (actual)
          filterNodes.push(actual)
      }
    } else {
      for (const ws of this[_workspaces]) {
        const ideal = this.idealTree.children.get(ws)
        if (ideal)
          filterNodes.push(ideal)
        const actual = this.actualTree.children.get(ws)
        if (actual)
          filterNodes.push(actual)
      }
    }

    // find all the nodes that need to change between the actual
    // and ideal trees.
    this.diff = Diff.calculate({
      shrinkwrapInflated: this[_shrinkwrapInflated],
      filterNodes,
      actual: this.actualTree,
      ideal: this.idealTree,
    })

    // we don't have to add 'removed' folders to the trashlist, because
    // they'll be moved aside to a retirement folder, and then the retired
    // folder will be deleted at the end.  This is important when we have
    // a folder like FOO being "removed" in favor of a folder like "foo",
    // because if we remove node_modules/FOO on case-insensitive systems,
    // it will remove the dep that we *want* at node_modules/foo.

    process.emit('timeEnd', 'reify:diffTrees')
  }

  // add the node and all its bins to the list of things to be
  // removed later on in the process.  optionally, also mark them
  // as a retired paths, so that we move them out of the way and
  // replace them when rolling back on failure.
  [_addNodeToTrashList] (node, retire = false) {
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
      node.top.isProjectRoot &&
        (node.peer && this[_omitPeer] ||
          node.dev && this[_omitDev] ||
          node.optional && this[_omitOptional] ||
          node.devOptional && this[_omitOptional] && this[_omitDev])

    for (const node of this.idealTree.inventory.filter(filter))
      this[_addNodeToTrashList](node)

    process.emit('timeEnd', 'reify:trashOmits')
  }

  [_createSparseTree] () {
    process.emit('time', 'reify:createSparse')
    // if we call this fn again, we look for the previous list
    // so that we can avoid making the same directory multiple times
    const leaves = this.diff.leaves
      .filter(diff => {
        return (diff.action === 'ADD' || diff.action === 'CHANGE') &&
          !this[_sparseTreeDirs].has(diff.ideal.path) &&
          !diff.ideal.isLink
      })
      .map(diff => diff.ideal)

    // we check this in parallel, so guard against multiple attempts to
    // retire the same path at the same time.
    const dirsChecked = new Set()
    return promiseAllRejectLate(leaves.map(async node => {
      for (const d of walkUp(node.path)) {
        if (d === node.top.path)
          break
        if (dirsChecked.has(d))
          continue
        dirsChecked.add(d)
        const st = await lstat(d).catch(er => null)
        // this can happen if we have a link to a package with a name
        // that the filesystem treats as if it is the same thing.
        // would be nice to have conditional istanbul ignores here...
        /* istanbul ignore next - defense in depth */
        if (st && !st.isDirectory()) {
          const retired = retirePath(d)
          this[_retiredPaths][d] = retired
          this[_trashList].add(retired)
          await this[_renamePath](d, retired)
        }
      }
      const made = await mkdirp(node.path)
      this[_sparseTreeDirs].add(node.path)
      this[_sparseTreeRoots].add(made)
    }))
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
    const seen = this[_shrinkwrapInflated]
    const shrinkwraps = this.diff.leaves
      .filter(d => (d.action === 'CHANGE' || d.action === 'ADD' || !d.action) &&
        d.ideal.hasShrinkwrap && !seen.has(d.ideal) &&
        !this[_trashList].has(d.ideal.path))

    if (!shrinkwraps.length)
      return

    process.emit('time', 'reify:loadShrinkwraps')

    const Arborist = this.constructor
    return promiseAllRejectLate(shrinkwraps.map(diff => {
      const node = diff.ideal
      seen.add(node)
      return diff.action ? this[_reifyNode](node) : node
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
      .then(async () => {
        // when we reify an optional node, check the engine and platform
        // first. be sure to ignore the --force and --engine-strict flags,
        // since we always want to skip any optional packages we can't install.
        // these checks throwing will result in a rollback and removal
        // of the mismatches
        if (node.optional) {
          checkEngine(node.package, npmVersion, nodeVersion, false)
          checkPlatform(node.package, false)
        }
        await this[_checkBins](node)
        await this[_extractOrLink](node)
        await this[_warnDeprecated](node)
      })

    return this[_handleOptionalFailure](node, p)
      .then(() => {
        this.finishTracker('reify', node.name, node.location)
        process.emit('timeEnd', timer)
        return node
      })
  }

  // do not allow node_modules to be a symlink
  async [_validateNodeModules] (nm) {
    if (this[_force] || this[_nmValidated].has(nm))
      return
    const st = await lstat(nm).catch(() => null)
    if (!st || st.isDirectory()) {
      this[_nmValidated].add(nm)
      return
    }
    this.log.warn('reify', 'Removing non-directory', nm)
    await rimraf(nm)
  }

  async [_extractOrLink] (node) {
    // in normal cases, node.resolved should *always* be set by now.
    // however, it is possible when a lockfile is damaged, or very old,
    // or in some other race condition bugs in npm v6, that a previously
    // bundled dependency will have just a version, but no resolved value,
    // and no 'bundled: true' setting.
    // Do the best with what we have, or else remove it from the tree
    // entirely, since we can't possibly reify it.
    const res = node.resolved ? `${node.name}@${this[_registryResolved](node.resolved)}`
      : node.packageName && node.version
        ? `${node.packageName}@${node.version}`
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

    const nm = resolve(node.parent.path, 'node_modules')
    await this[_validateNodeModules](nm)

    if (node.isLink) {
      await rimraf(node.path)
      await this[_symlink](node)
    } else {
      await debug(async () => {
        const st = await lstat(node.path).catch(e => null)
        if (st && !st.isDirectory()) {
          debug.log('unpacking into a non-directory', node)
          throw Object.assign(new Error('ENOTDIR: not a directory'), {
            code: 'ENOTDIR',
            path: node.path,
          })
        }
      })
      await pacote.extract(res, node.path, {
        ...this.options,
        resolved: node.resolved,
        integrity: node.integrity,
      })
    }
  }

  async [_symlink] (node) {
    const dir = dirname(node.path)
    const target = node.realpath
    const rel = relative(dir, target)
    await mkdirp(dir)
    return symlink(rel, node.path, 'junction')
  }

  [_warnDeprecated] (node) {
    const {_id, deprecated} = node.package
    if (deprecated)
      this.log.warn('deprecated', `${_id}: ${deprecated}`)
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
        node.target !== node.root &&
        !this[_trashList].has(node.path))

    if (!set.length)
      return this[_loadBundlesAndUpdateTrees](depth + 1, bundlesByDepth)

    // extract all the nodes with bundles
    return promiseAllRejectLate(set.map(node => {
      this[_bundleUnpacked].add(node)
      return this[_reifyNode](node)
    }))
    // then load their unpacked children and move into the ideal tree
      .then(nodes =>
        promiseAllRejectLate(nodes.map(async node => {
          const arb = new this.constructor({
            ...this.options,
            path: node.path,
          })
          const notTransplanted = new Set(node.children.keys())
          await arb.loadActual({
            root: node,
            // don't transplant any sparse folders we created
            // loadActual will set node.package to {} for empty directories
            // if by chance there are some empty folders in the node_modules
            // tree for some other reason, then ok, ignore those too.
            transplantFilter: node => {
              if (node.package._id) {
                // it's actually in the bundle if it gets transplanted
                notTransplanted.delete(node.name)
                return true
              } else
                return false
            },
          })
          for (const name of notTransplanted)
            this[_bundleMissing].add(node.children.get(name))
        })))
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
        if (!node)
          return
        if (node.isProjectRoot)
          return

        const { bundleDependencies } = node.package
        if (bundleDependencies && bundleDependencies.length) {
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

    // Example dep graph:
    // root -> (a, c)
    // a -> BUNDLE(b)
    // b -> c
    // c -> b
    //
    // package tree:
    // root
    // +-- a
    // |   +-- b(1)
    // |   +-- c(1)
    // +-- b(2)
    // +-- c(2)
    // 1. mark everything that's shadowed by anything in the bundle.  This
    //    marks b(2) and c(2).
    // 2. anything with edgesIn from outside the set, mark not-extraneous,
    //    remove from set.  This unmarks c(2).
    // 3. continue until no change
    // 4. remove everything in the set from the tree.  b(2) is pruned

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

    // lib -> (a@1.x) BUNDLE(a@1.2.3 (b@1.2.3))
    // a@1.2.3 -> (b@1.2.3)
    // a@1.3.0 -> (b@2)
    // b@1.2.3 -> ()
    // b@2 -> (c@2)
    //
    // root
    // +-- lib
    // |   +-- a@1.2.3
    // |   +-- b@1.2.3
    // +-- b@2 <-- shadowed, now extraneous
    // +-- c@2 <-- also shadowed, because only dependent is shadowed
    for (const shadow of bundleShadowed) {
      for (const shadDep of shadow.edgesOut.values()) {
        /* istanbul ignore else - pretty unusual situation, just being
         * defensive here. Would mean that a bundled dep has a dependency
         * that is unmet. which, weird, but if you bundle it, we take
         * whatever you put there and assume the publisher knows best. */
        if (shadDep.to) {
          bundleShadowed.add(shadDep.to)
          shadDep.to.extraneous = true
        }
      }
    }

    let changed
    do {
      changed = false
      for (const shadow of bundleShadowed) {
        for (const edge of shadow.edgesIn) {
          if (!bundleShadowed.has(edge.from)) {
            shadow.extraneous = false
            bundleShadowed.delete(shadow)
            changed = true
            break
          }
        }
      }
    } while (changed)

    for (const shadow of bundleShadowed) {
      this[_addNodeToTrashList](shadow)
      shadow.root = null
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
    const options = { ...this.options }
    const tree = this.idealTree

    // if we're operating on a workspace, only audit the workspace deps
    if (this[_workspaces] && this[_workspaces].length)
      options.filterSet = this.workspaceDependencySet(tree, this[_workspaces])

    this.auditReport = AuditReport.load(tree, options)
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
        const bd = this[_bundleUnpacked].has(node)
        const sw = this[_shrinkwrapInflated].has(node)
        const bundleMissing = this[_bundleMissing].has(node)

        // check whether we still need to unpack this one.
        // test the inDepBundle last, since that's potentially a tree walk.
        const doUnpack = node && // can't unpack if removed!
          !node.isRoot && // root node already exists
          !bd && // already unpacked to read bundle
          !sw && // already unpacked to read sw
          (bundleMissing || !node.inDepBundle) // already unpacked by another dep's bundle

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
      // skip if nothing was retired
      if (diff.action !== 'CHANGE' && diff.action !== 'REMOVE')
        return

      const { path: realFolder } = diff.actual
      const retireFolder = moves[realFolder]
      /* istanbul ignore next - should be impossible */
      debug(() => {
        if (!retireFolder) {
          const er = new Error('trying to un-retire but not retired')
          throw Object.assign(er, {
            realFolder,
            retireFolder,
            actual: diff.actual,
            ideal: diff.ideal,
            action: diff.action,
          })
        }
      })

      this[_retiredUnchanged][retireFolder] = []
      return promiseAllRejectLate(diff.unchanged.map(node => {
        // no need to roll back links, since we'll just delete them anyway
        if (node.isLink)
          return mkdirp(dirname(node.path)).then(() => this[_reifyNode](node))

        // will have been moved/unpacked along with bundler
        if (node.inDepBundle && !this[_bundleMissing].has(node))
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

    // pick up link nodes from the unchanged list as we want to run their
    // scripts in every install despite of having a diff status change
    for (const node of this.diff.unchanged) {
      const tree = node.root.target

      // skip links that only live within node_modules as they are most
      // likely managed by packages we installed, we only want to rebuild
      // unchanged links we directly manage
      if (node.isLink && node.target.fsTop === tree)
        nodes.push(node)
    }

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
  async [_saveIdealTree] (options) {
    // the ideal tree is actualized now, hooray!
    // it still contains all the references to optional nodes that were removed
    // for install failures.  Those still end up in the shrinkwrap, so we
    // save it first, then prune out the optional trash, and then return it.

    // support save=false option
    if (options.save === false || this[_global] || this[_dryRun])
      return false

    process.emit('time', 'reify:save')

    const updatedTrees = new Set()

    // resolvedAdd is the list of user add requests, but with names added
    // to things like git repos and tarball file/urls.  However, if the
    // user requested 'foo@', and we have a foo@file:../foo, then we should
    // end up saving the spec we actually used, not whatever they gave us.
    if (this[_resolvedAdd].length) {
      for (const { name, tree: addTree } of this[_resolvedAdd]) {
        // addTree either the root, or a workspace
        const edge = addTree.edgesOut.get(name)
        const pkg = addTree.package
        const req = npa.resolve(name, edge.spec, addTree.realpath)
        const {rawSpec, subSpec} = req

        const spec = subSpec ? subSpec.rawSpec : rawSpec
        const child = edge.to

        let newSpec
        if (req.registry) {
          const version = child.version
          const prefixRange = version ? this[_savePrefix] + version : '*'
          // if we installed a range, then we save the range specified
          // if it is not a subset of the ^x.y.z.  eg, installing a range
          // of `1.x <1.2.3` will not be saved as `^1.2.0`, because that
          // would allow versions outside the requested range.  Tags and
          // specific versions save with the save-prefix.
          const isRange = (subSpec || req).type === 'range'

          let range = spec
          if (
            !isRange ||
            spec === '*' ||
            subset(prefixRange, spec, { loose: true })
          )
            range = prefixRange

          const pname = child.packageName
          const alias = name !== pname
          newSpec = alias ? `npm:${pname}@${range}` : range
        } else if (req.hosted) {
          // save the git+https url if it has auth, otherwise shortcut
          const h = req.hosted
          const opt = { noCommittish: false }
          if (h.https && h.auth)
            newSpec = `git+${h.https(opt)}`
          else
            newSpec = h.shortcut(opt)
        } else if (req.type === 'directory' || req.type === 'file') {
          // save the relative path in package.json
          // Normally saveSpec is updated with the proper relative
          // path already, but it's possible to specify a full absolute
          // path initially, in which case we can end up with the wrong
          // thing, so just get the ultimate fetchSpec and relativize it.
          const p = req.fetchSpec.replace(/^file:/, '')
          const rel = relpath(addTree.realpath, p)
          newSpec = `file:${rel}`
        } else
          newSpec = req.saveSpec

        if (options.saveType) {
          const depType = saveTypeMap.get(options.saveType)
          pkg[depType][name] = newSpec
          // rpj will have moved it here if it was in both
          // if it is empty it will be deleted later
          if (options.saveType === 'prod' && pkg.optionalDependencies)
            delete pkg.optionalDependencies[name]
        } else {
          if (hasSubKey(pkg, 'dependencies', name))
            pkg.dependencies[name] = newSpec

          if (hasSubKey(pkg, 'devDependencies', name)) {
            pkg.devDependencies[name] = newSpec
            // don't update peer or optional if we don't have to
            if (hasSubKey(pkg, 'peerDependencies', name) && !intersects(newSpec, pkg.peerDependencies[name]))
              pkg.peerDependencies[name] = newSpec

            if (hasSubKey(pkg, 'optionalDependencies', name) && !intersects(newSpec, pkg.optionalDependencies[name]))
              pkg.optionalDependencies[name] = newSpec
          } else {
            if (hasSubKey(pkg, 'peerDependencies', name))
              pkg.peerDependencies[name] = newSpec

            if (hasSubKey(pkg, 'optionalDependencies', name))
              pkg.optionalDependencies[name] = newSpec
          }
        }

        updatedTrees.add(addTree)
      }
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

    const promises = [this[_saveLockFile](saveOpt)]

    const updatePackageJson = async (tree) => {
      const pkgJson = await PackageJson.load(tree.path)
        .catch(() => new PackageJson(tree.path))
      const {
        dependencies = {},
        devDependencies = {},
        optionalDependencies = {},
        peerDependencies = {},
      } = tree.package

      pkgJson.update({
        dependencies,
        devDependencies,
        optionalDependencies,
        peerDependencies,
      })
      await pkgJson.save()
    }

    // grab any from explicitRequests that had deps removed
    for (const { from: tree } of this.explicitRequests)
      updatedTrees.add(tree)

    for (const tree of updatedTrees) {
      // refresh the edges so they have the correct specs
      tree.package = tree.package
      promises.push(updatePackageJson(tree))
    }

    await Promise.all(promises)
    process.emit('timeEnd', 'reify:save')
    return true
  }

  async [_saveLockFile] (saveOpt) {
    if (!this[_usePackageLock])
      return

    const { meta } = this.idealTree

    return meta.save(saveOpt)
  }

  async [_copyIdealToActual] () {
    // clean up any trash that is still in the tree
    for (const path of this[_trashList]) {
      const loc = relpath(this.idealTree.realpath, path)
      const node = this.idealTree.inventory.get(loc)
      if (node && node.root === this.idealTree)
        node.parent = null
    }

    // if we filtered to only certain nodes, then anything ELSE needs
    // to be untouched in the resulting actual tree, even if it differs
    // in the idealTree.  Copy over anything that was in the actual and
    // was not changed, delete anything in the ideal and not actual.
    // Then we move the entire idealTree over to this.actualTree, and
    // save the hidden lockfile.
    if (this.diff && this.diff.filterSet.size) {
      const reroot = new Set()

      const { filterSet } = this.diff
      const seen = new Set()
      for (const [loc, ideal] of this.idealTree.inventory.entries()) {
        seen.add(loc)

        // if it's an ideal node from the filter set, then skip it
        // because we already made whatever changes were necessary
        if (filterSet.has(ideal))
          continue

        // otherwise, if it's not in the actualTree, then it's not a thing
        // that we actually added.  And if it IS in the actualTree, then
        // it's something that we left untouched, so we need to record
        // that.
        const actual = this.actualTree.inventory.get(loc)
        if (!actual)
          ideal.root = null
        else {
          if ([...actual.linksIn].some(link => filterSet.has(link))) {
            seen.add(actual.location)
            continue
          }
          const { realpath, isLink } = actual
          if (isLink && ideal.isLink && ideal.realpath === realpath)
            continue
          else
            reroot.add(actual)
        }
      }

      // now find any actual nodes that may not be present in the ideal
      // tree, but were left behind by virtue of not being in the filter
      for (const [loc, actual] of this.actualTree.inventory.entries()) {
        if (seen.has(loc))
          continue
        seen.add(loc)

        // we know that this is something that ISN'T in the idealTree,
        // or else we will have addressed it in the previous loop.
        // If it's in the filterSet, that means we intentionally removed
        // it, so nothing to do here.
        if (filterSet.has(actual))
          continue

        reroot.add(actual)
      }

      // go through the rerooted actual nodes, and move them over.
      for (const actual of reroot)
        actual.root = this.idealTree

      // prune out any tops that lack a linkIn, they are no longer relevant.
      for (const top of this.idealTree.tops) {
        if (top.linksIn.size === 0)
          top.root = null
      }

      // need to calculate dep flags, since nodes may have been marked
      // as extraneous or otherwise incorrect during transit.
      calcDepFlags(this.idealTree)
    }

    // save the ideal's meta as a hidden lockfile after we actualize it
    this.idealTree.meta.filename =
      this.idealTree.realpath + '/node_modules/.package-lock.json'
    this.idealTree.meta.hiddenLockfile = true

    this.actualTree = this.idealTree
    this.idealTree = null

    if (!this[_global])
      await this.actualTree.meta.save()
  }
}
