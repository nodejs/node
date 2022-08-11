// mix-in implementing the loadActual method

const { relative, dirname, resolve, join, normalize } = require('path')

const rpj = require('read-package-json-fast')
const { promisify } = require('util')
const readdir = promisify(require('readdir-scoped-modules'))
const walkUp = require('walk-up-path')
const ancestorPath = require('common-ancestor-path')
const treeCheck = require('../tree-check.js')

const Shrinkwrap = require('../shrinkwrap.js')
const calcDepFlags = require('../calc-dep-flags.js')
const Node = require('../node.js')
const Link = require('../link.js')
const realpath = require('../realpath.js')

const _loadFSNode = Symbol('loadFSNode')
const _newNode = Symbol('newNode')
const _newLink = Symbol('newLink')
const _loadFSTree = Symbol('loadFSTree')
const _loadFSChildren = Symbol('loadFSChildren')
const _findMissingEdges = Symbol('findMissingEdges')
const _findFSParents = Symbol('findFSParents')
const _resetDepFlags = Symbol('resetDepFlags')

const _actualTreeLoaded = Symbol('actualTreeLoaded')
const _rpcache = Symbol.for('realpathCache')
const _stcache = Symbol.for('statCache')
const _topNodes = Symbol('linkTargets')
const _cache = Symbol('nodeLoadingCache')
const _loadActual = Symbol('loadActual')
const _loadActualVirtually = Symbol('loadActualVirtually')
const _loadActualActually = Symbol('loadActualActually')
const _loadWorkspaces = Symbol.for('loadWorkspaces')
const _loadWorkspaceTargets = Symbol('loadWorkspaceTargets')
const _actualTreePromise = Symbol('actualTreePromise')
const _actualTree = Symbol('actualTree')
const _transplant = Symbol('transplant')
const _transplantFilter = Symbol('transplantFilter')

const _filter = Symbol('filter')
const _global = Symbol.for('global')
const _changePath = Symbol.for('_changePath')

module.exports = cls => class ActualLoader extends cls {
  constructor (options) {
    super(options)

    this[_global] = !!options.global

    // the tree of nodes on disk
    this.actualTree = options.actualTree

    // ensure when walking the tree that we don't call loadTree on the
    // same actual node more than one time.
    this[_actualTreeLoaded] = new Set()

    // caches for cached realpath calls
    const cwd = process.cwd()
    // assume that the cwd is real enough for our purposes
    this[_rpcache] = new Map([[cwd, cwd]])
    this[_stcache] = new Map()

    // cache of nodes when loading the actualTree, so that we avoid
    // loaded the same node multiple times when symlinks attack.
    this[_cache] = new Map()

    // cache of link targets for setting fsParent links
    // We don't do fsParent as a magic getter/setter, because
    // it'd be too costly to keep up to date along the walk.
    // And, we know that it can ONLY be relevant when the node
    // is a target of a link, otherwise it'd be in a node_modules
    // folder, so take advantage of that to limit the scans later.
    this[_topNodes] = new Set()
  }

  [_resetDepFlags] (tree, root) {
    // reset all deps to extraneous prior to recalc
    if (!root) {
      for (const node of tree.inventory.values()) {
        node.extraneous = true
      }
    }

    // only reset root flags if we're not re-rooting,
    // otherwise leave as-is
    calcDepFlags(tree, !root)
    return tree
  }

  // public method
  async loadActual (options = {}) {
    // allow the user to set options on the ctor as well.
    // XXX: deprecate separate method options objects.
    options = { ...this.options, ...options }

    // stash the promise so that we don't ever have more than one
    // going at the same time.  This is so that buildIdealTree can
    // default to the actualTree if no shrinkwrap present, but
    // reify() can still call buildIdealTree and loadActual in parallel
    // safely.
    return this.actualTree ? this.actualTree
      : this[_actualTreePromise] ? this[_actualTreePromise]
      : this[_actualTreePromise] = this[_loadActual](options)
        .then(tree => this[_resetDepFlags](tree, options.root))
        .then(tree => this.actualTree = treeCheck(tree))
  }

  async [_loadActual] (options) {
    // mostly realpath to throw if the root doesn't exist
    const {
      global = false,
      filter = () => true,
      root = null,
      transplantFilter = () => true,
      ignoreMissing = false,
      forceActual = false,
    } = options
    this[_filter] = filter
    this[_transplantFilter] = transplantFilter

    if (global) {
      const real = await realpath(this.path, this[_rpcache], this[_stcache])
      const newNodeOrLink = this.path === real ? _newNode : _newLink
      this[_actualTree] = await this[newNodeOrLink]({
        path: this.path,
        realpath: real,
        pkg: {},
        global,
        loadOverrides: true,
      })
      return this[_loadActualActually]({ root, ignoreMissing, global })
    }

    // not in global mode, hidden lockfile is allowed, load root pkg too
    this[_actualTree] = await this[_loadFSNode]({
      path: this.path,
      real: await realpath(this.path, this[_rpcache], this[_stcache]),
      loadOverrides: true,
    })

    this[_actualTree].assertRootOverrides()

    // if forceActual is set, don't even try the hidden lockfile
    if (!forceActual) {
      // Note: hidden lockfile will be rejected if it's not the latest thing
      // in the folder, or if any of the entries in the hidden lockfile are
      // missing.
      const meta = await Shrinkwrap.load({
        path: this[_actualTree].path,
        hiddenLockfile: true,
        resolveOptions: this.options,
      })

      if (meta.loadedFromDisk) {
        this[_actualTree].meta = meta
        return this[_loadActualVirtually]({ root })
      }
    }

    const meta = await Shrinkwrap.load({
      path: this[_actualTree].path,
      lockfileVersion: this.options.lockfileVersion,
      resolveOptions: this.options,
    })
    this[_actualTree].meta = meta
    return this[_loadActualActually]({ root, ignoreMissing })
  }

  async [_loadActualVirtually] ({ root }) {
    // have to load on a new Arborist object, so we don't assign
    // the virtualTree on this one!  Also, the weird reference is because
    // we can't easily get a ref to Arborist in this module, without
    // creating a circular reference, since this class is a mixin used
    // to build up the Arborist class itself.
    await new this.constructor({ ...this.options }).loadVirtual({
      root: this[_actualTree],
    })
    await this[_loadWorkspaces](this[_actualTree])

    this[_transplant](root)
    return this[_actualTree]
  }

  async [_loadActualActually] ({ root, ignoreMissing, global }) {
    await this[_loadFSTree](this[_actualTree])
    await this[_loadWorkspaces](this[_actualTree])
    await this[_loadWorkspaceTargets](this[_actualTree])
    if (!ignoreMissing) {
      await this[_findMissingEdges]()
    }
    this[_findFSParents]()
    this[_transplant](root)

    if (global) {
      // need to depend on the children, or else all of them
      // will end up being flagged as extraneous, since the
      // global root isn't a "real" project
      const tree = this[_actualTree]
      const actualRoot = tree.isLink ? tree.target : tree
      const { dependencies = {} } = actualRoot.package
      for (const [name, kid] of actualRoot.children.entries()) {
        const def = kid.isLink ? `file:${kid.realpath.replace(/#/g, '%23')}` : '*'
        dependencies[name] = dependencies[name] || def
      }
      actualRoot.package = { ...actualRoot.package, dependencies }
    }
    return this[_actualTree]
  }

  // if there are workspace targets without Link nodes created, load
  // the targets, so that we know what they are.
  async [_loadWorkspaceTargets] (tree) {
    if (!tree.workspaces || !tree.workspaces.size) {
      return
    }

    const promises = []
    for (const path of tree.workspaces.values()) {
      if (!this[_cache].has(path)) {
        // workspace overrides use the root overrides
        const p = this[_loadFSNode]({ path, root: this[_actualTree], useRootOverrides: true })
          .then(node => this[_loadFSTree](node))
        promises.push(p)
      }
    }
    await Promise.all(promises)
  }

  [_transplant] (root) {
    if (!root || root === this[_actualTree]) {
      return
    }

    this[_actualTree][_changePath](root.path)
    for (const node of this[_actualTree].children.values()) {
      if (!this[_transplantFilter](node)) {
        node.root = null
      }
    }

    root.replace(this[_actualTree])
    for (const node of this[_actualTree].fsChildren) {
      node.root = this[_transplantFilter](node) ? root : null
    }

    this[_actualTree] = root
  }

  [_loadFSNode] ({ path, parent, real, root, loadOverrides, useRootOverrides }) {
    if (!real) {
      return realpath(path, this[_rpcache], this[_stcache])
        .then(
          real => this[_loadFSNode]({
            path,
            parent,
            real,
            root,
            loadOverrides,
            useRootOverrides,
          }),
          // if realpath fails, just provide a dummy error node
          error => new Node({
            error,
            path,
            realpath: path,
            parent,
            root,
            loadOverrides,
          })
        )
    }

    // cache temporarily holds a promise placeholder so we don't try to create
    // the same node multiple times.  this is rare to encounter, given the
    // aggressive caching on realpath and lstat calls, but it's possible that
    // it's already loaded as a tree top, and then gets its parent loaded
    // later, if a symlink points deeper in the tree.
    const cached = this[_cache].get(path)
    if (cached && !cached.dummy) {
      return Promise.resolve(cached).then(node => {
        node.parent = parent
        return node
      })
    }

    const p = rpj(join(real, 'package.json'))
      // soldier on if read-package-json raises an error
      .then(pkg => [pkg, null], error => [null, error])
      .then(([pkg, error]) => {
        return this[normalize(path) === real ? _newNode : _newLink]({
          installLinks: this.installLinks,
          legacyPeerDeps: this.legacyPeerDeps,
          path,
          realpath: real,
          pkg,
          error,
          parent,
          root,
          loadOverrides,
          ...(useRootOverrides && root.overrides
            ? { overrides: root.overrides.getNodeRule({ name: pkg.name, version: pkg.version }) }
            : {}),
        })
      })
      .then(node => {
        this[_cache].set(path, node)
        return node
      })

    this[_cache].set(path, p)
    return p
  }

  // this is the way it is to expose a timing issue which is difficult to
  // test otherwise.  The creation of a Node may take slightly longer than
  // the creation of a Link that targets it.  If the Node has _begun_ its
  // creation phase (and put a Promise in the cache) then the Link will
  // get a Promise as its cachedTarget instead of an actual Node object.
  // This is not a problem, because it gets resolved prior to returning
  // the tree or attempting to load children.  However, it IS remarkably
  // difficult to get to happen in a test environment to verify reliably.
  // Hence this kludge.
  [_newNode] (options) {
    // check it for an fsParent if it's a tree top.  there's a decent chance
    // it'll get parented later, making the fsParent scan a no-op, but better
    // safe than sorry, since it's cheap.
    const { parent, realpath } = options
    if (!parent) {
      this[_topNodes].add(realpath)
    }
    return process.env._TEST_ARBORIST_SLOW_LINK_TARGET_ === '1'
      ? new Promise(res => setTimeout(() => res(new Node(options)), 100))
      : new Node(options)
  }

  [_newLink] (options) {
    const { realpath } = options
    this[_topNodes].add(realpath)
    const target = this[_cache].get(realpath)
    const link = new Link({ ...options, target })

    if (!target) {
      this[_cache].set(realpath, link.target)
      // if a link target points at a node outside of the root tree's
      // node_modules hierarchy, then load that node as well.
      return this[_loadFSTree](link.target).then(() => link)
    } else if (target.then) {
      target.then(node => link.target = node)
    }

    return link
  }

  [_loadFSTree] (node) {
    const did = this[_actualTreeLoaded]
    node = node.target

    // if a Link target has started, but not completed, then
    // a Promise will be in the cache to indicate this.
    if (node.then) {
      return node.then(node => this[_loadFSTree](node))
    }

    // impossible except in pathological ELOOP cases
    /* istanbul ignore if */
    if (did.has(node.realpath)) {
      return Promise.resolve(node)
    }

    did.add(node.realpath)
    return this[_loadFSChildren](node)
      .then(() => Promise.all(
        [...node.children.entries()]
          .filter(([name, kid]) => !did.has(kid.realpath))
          .map(([name, kid]) => this[_loadFSTree](kid))))
  }

  // create child nodes for all the entries in node_modules
  // and attach them to the node as a parent
  [_loadFSChildren] (node) {
    const nm = resolve(node.realpath, 'node_modules')
    return readdir(nm).then(kids => {
      return Promise.all(
      // ignore . dirs and retired scoped package folders
        kids.filter(kid => !/^(@[^/]+\/)?\./.test(kid))
          .filter(kid => this[_filter](node, kid))
          .map(kid => this[_loadFSNode]({
            parent: node,
            path: resolve(nm, kid),
          })))
    },
    // error in the readdir is not fatal, just means no kids
    () => {})
  }

  async [_findMissingEdges] () {
    // try to resolve any missing edges by walking up the directory tree,
    // checking for the package in each node_modules folder.  stop at the
    // root directory.
    // The tricky move here is that we load a "dummy" node for the folder
    // containing the node_modules folder, so that it can be assigned as
    // the fsParent.  It's a bad idea to *actually* load that full node,
    // because people sometimes develop in ~/projects/node_modules/...
    // so we'd end up loading a massive tree with lots of unrelated junk.
    const nmContents = new Map()
    const tree = this[_actualTree]
    for (const node of tree.inventory.values()) {
      const ancestor = ancestorPath(node.realpath, this.path)

      const depPromises = []
      for (const [name, edge] of node.edgesOut.entries()) {
        const notMissing = !edge.missing &&
          !(edge.to && (edge.to.dummy || edge.to.parent !== node))
        if (notMissing) {
          continue
        }

        // start the walk from the dirname, because we would have found
        // the dep in the loadFSTree step already if it was local.
        for (const p of walkUp(dirname(node.realpath))) {
          // only walk as far as the nearest ancestor
          // this keeps us from going into completely unrelated
          // places when a project is just missing something, but
          // allows for finding the transitive deps of link targets.
          // ie, if it has to go up and back out to get to the path
          // from the nearest common ancestor, we've gone too far.
          if (ancestor && /^\.\.(?:[\\/]|$)/.test(relative(ancestor, p))) {
            break
          }

          const entries = nmContents.get(p) ||
            await readdir(p + '/node_modules').catch(() => [])
          nmContents.set(p, entries)
          if (!entries.includes(name)) {
            continue
          }

          const d = this[_cache].has(p) ? await this[_cache].get(p)
            : new Node({ path: p, root: node.root, dummy: true })
          this[_cache].set(p, d)
          if (d.dummy) {
            // it's a placeholder, so likely would not have loaded this dep,
            // unless another dep in the tree also needs it.
            const depPath = normalize(`${p}/node_modules/${name}`)
            const cached = this[_cache].get(depPath)
            if (!cached || cached.dummy) {
              depPromises.push(this[_loadFSNode]({
                path: depPath,
                root: node.root,
                parent: d,
              }).then(node => this[_loadFSTree](node)))
            }
          }
          break
        }
      }
      await Promise.all(depPromises)
    }
  }

  // try to find a node that is the parent in a fs tree sense, but not a
  // node_modules tree sense, of any link targets.  this allows us to
  // resolve deps that node will find, but a legacy npm view of the
  // world would not have noticed.
  [_findFSParents] () {
    for (const path of this[_topNodes]) {
      const node = this[_cache].get(path)
      if (node && !node.parent && !node.fsParent) {
        for (const p of walkUp(dirname(path))) {
          if (this[_cache].has(p)) {
            node.fsParent = this[_cache].get(p)
            break
          }
        }
      }
    }
  }
}
