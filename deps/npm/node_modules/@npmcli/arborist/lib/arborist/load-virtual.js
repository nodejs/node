const { resolve } = require('node:path')
// mixin providing the loadVirtual method
const mapWorkspaces = require('@npmcli/map-workspaces')
const PackageJson = require('@npmcli/package-json')
const nameFromFolder = require('@npmcli/name-from-folder')

const consistentResolve = require('../consistent-resolve.js')
const Shrinkwrap = require('../shrinkwrap.js')
const Node = require('../node.js')
const Link = require('../link.js')
const relpath = require('../relpath.js')
const calcDepFlags = require('../calc-dep-flags.js')
const treeCheck = require('../tree-check.js')

const flagsSuspect = Symbol.for('flagsSuspect')
const setWorkspaces = Symbol.for('setWorkspaces')

module.exports = cls => class VirtualLoader extends cls {
  #rootOptionProvided

  // public method
  async loadVirtual (options = {}) {
    if (this.virtualTree) {
      return this.virtualTree
    }

    // allow the user to set reify options on the ctor as well.
    // XXX: deprecate separate reify() options object.
    options = { ...this.options, ...options }

    if (options.root && options.root.meta) {
      await this.#loadFromShrinkwrap(options.root.meta, options.root)
      return treeCheck(this.virtualTree)
    }

    const s = await Shrinkwrap.load({
      path: this.path,
      lockfileVersion: this.options.lockfileVersion,
      resolveOptions: this.options,
    })
    if (!s.loadedFromDisk && !options.root) {
      const er = new Error('loadVirtual requires existing shrinkwrap file')
      throw Object.assign(er, { code: 'ENOLOCK' })
    }

    // when building the ideal tree, we pass in a root node to this function
    // otherwise, load it from the root package json or the lockfile
    const pkg = await PackageJson.normalize(this.path).then(p => p.content).catch(() => s.data.packages[''] || {})
    // TODO clean this up
    const {
      root = await this[setWorkspaces](this.#loadNode('', pkg, true)),
    } = options
    this.#rootOptionProvided = options.root

    await this.#loadFromShrinkwrap(s, root)
    root.assertRootOverrides()
    return treeCheck(this.virtualTree)
  }

  async #loadFromShrinkwrap (s, root) {
    if (!this.#rootOptionProvided) {
      // root is never any of these things, but might be a brand new
      // baby Node object that never had its dep flags calculated.
      root.unsetDepFlags()
    } else {
      this[flagsSuspect] = true
    }

    this.#checkRootEdges(s, root)
    root.meta = s
    this.virtualTree = root
    // separate out link metadata, and create Node objects for nodes
    const links = new Map()
    const nodes = new Map([['', root]])
    for (const [location, meta] of Object.entries(s.data.packages)) {
      // skip the root because we already got it
      if (!location) {
        continue
      }

      if (meta.link) {
        links.set(location, meta)
      } else {
        nodes.set(location, this.#loadNode(location, meta))
      }
    }
    await this.#resolveLinks(links, nodes)
    if (!(s.originalLockfileVersion >= 2)) {
      this.#assignBundles(nodes)
    }
    if (this[flagsSuspect]) {
      // reset all dep flags
      // can't use inventory here, because virtualTree might not be root
      for (const node of nodes.values()) {
        if (node.isRoot || node === this.#rootOptionProvided) {
          continue
        }
        node.resetDepFlags()
      }
      calcDepFlags(this.virtualTree, !this.#rootOptionProvided)
    }
    return root
  }

  // check the lockfile deps, and see if they match.  if they do not
  // then we have to reset dep flags at the end.  for example, if the
  // user manually edits their package.json file, then we need to know
  // that the idealTree is no longer entirely trustworthy.
  #checkRootEdges (s, root) {
    // loaded virtually from tree, no chance of being out of sync
    // ancient lockfiles are critically damaged by this process,
    // so we need to just hope for the best in those cases.
    if (!s.loadedFromDisk || s.ancientLockfile) {
      return
    }

    const lock = s.get('')
    const prod = lock.dependencies || {}
    const dev = lock.devDependencies || {}
    const optional = lock.optionalDependencies || {}
    const peer = lock.peerDependencies || {}
    const peerOptional = {}

    if (lock.peerDependenciesMeta) {
      for (const [name, meta] of Object.entries(lock.peerDependenciesMeta)) {
        if (meta.optional && peer[name] !== undefined) {
          peerOptional[name] = peer[name]
          delete peer[name]
        }
      }
    }

    for (const name of Object.keys(optional)) {
      delete prod[name]
    }

    const lockWS = {}
    const workspaces = mapWorkspaces.virtual({
      cwd: this.path,
      lockfile: s.data,
    })

    for (const [name, path] of workspaces.entries()) {
      lockWS[name] = `file:${path}`
    }

    // Should rootNames exclude optional?
    const rootNames = new Set(root.edgesOut.keys())

    const lockByType = ({ dev, optional, peer, peerOptional, prod, workspace: lockWS })

    // Find anything in shrinkwrap deps that doesn't match root's type or spec
    for (const type in lockByType) {
      const deps = lockByType[type]
      for (const name in deps) {
        const edge = root.edgesOut.get(name)
        if (!edge || edge.type !== type || edge.spec !== deps[name]) {
          return this[flagsSuspect] = true
        }
        rootNames.delete(name)
      }
    }
    // Something was in root that's not accounted for in shrinkwrap
    if (rootNames.size) {
      return this[flagsSuspect] = true
    }
  }

  // links is the set of metadata, and nodes is the map of non-Link nodes
  // Set the targets to nodes in the set, if we have them (we might not)
  // XXX build-ideal-tree also has a #resolveLinks, is there overlap?
  async #resolveLinks (links, nodes) {
    for (const [location, meta] of links.entries()) {
      const targetPath = resolve(this.path, meta.resolved)
      const targetLoc = relpath(this.path, targetPath)
      const target = nodes.get(targetLoc)

      if (!target) {
        const err = new Error(
`Missing target in lock file: "${targetLoc}" is referenced by "${location}" but does not exist.
To fix:
1. rm package-lock.json
2. npm install`
        )
        err.code = 'EMISSINGTARGET'
        throw err
      }

      const link = this.#loadLink(location, targetLoc, target, meta)
      nodes.set(location, link)
      nodes.set(targetLoc, link.target)

      // we always need to read the package.json for link targets
      // outside node_modules because they can be changed by the local user
      if (!link.target.parent) {
        await PackageJson.normalize(link.realpath).then(p => link.target.package = p.content).catch(() => null)
      }
    }
  }

  #assignBundles (nodes) {
    for (const [location, node] of nodes) {
      // Skip assignment of parentage for the root package
      if (!location || node.isLink && !node.target.location) {
        continue
      }
      const { name, parent, package: { inBundle } } = node

      if (!parent) {
        continue
      }

      // read inBundle from package because 'package' here is
      // actually a v2 lockfile metadata entry.
      // If the *parent* is also bundled, though, or if the parent has
      // no dependency on it, then we assume that it's being pulled in
      // just by virtue of its parent or a transitive dep being bundled.
      const { package: ppkg } = parent
      const { inBundle: parentBundled } = ppkg
      if (inBundle && !parentBundled && parent.edgesOut.has(node.name)) {
        if (!ppkg.bundleDependencies) {
          ppkg.bundleDependencies = [name]
        } else {
          ppkg.bundleDependencies.push(name)
        }
      }
    }
  }

  #loadNode (location, sw, loadOverrides) {
    const p = this.virtualTree ? this.virtualTree.realpath : this.path
    const path = resolve(p, location)
    // shrinkwrap doesn't include package name unless necessary
    if (!sw.name) {
      sw.name = nameFromFolder(path)
    }

    const node = new Node({
      installLinks: this.installLinks,
      legacyPeerDeps: this.legacyPeerDeps,
      root: this.virtualTree,
      path,
      realpath: path,
      integrity: sw.integrity,
      resolved: consistentResolve(sw.resolved, this.path, path),
      pkg: sw,
      hasShrinkwrap: sw.hasShrinkwrap,
      loadOverrides,
      // cast to boolean because they're undefined in the lock file when false
      extraneous: !!sw.extraneous,
      devOptional: !!(sw.devOptional || sw.dev || sw.optional),
      peer: !!sw.peer,
      optional: !!sw.optional,
      dev: !!sw.dev,
    })

    return node
  }

  #loadLink (location, targetLoc, target) {
    const path = resolve(this.path, location)
    const link = new Link({
      installLinks: this.installLinks,
      legacyPeerDeps: this.legacyPeerDeps,
      path,
      realpath: resolve(this.path, targetLoc),
      target,
      pkg: target && target.package,
    })
    link.extraneous = target.extraneous
    link.devOptional = target.devOptional
    link.peer = target.peer
    link.optional = target.optional
    link.dev = target.dev
    return link
  }
}
