const { mkdirSync } = require('node:fs')
const pacote = require('pacote')
const { join } = require('node:path')
const { depth } = require('treeverse')
const crypto = require('node:crypto')
const { IsolatedNode, IsolatedLink } = require('../isolated-classes.js')

// generate short hash key based on the dependency tree starting at this node
const getKey = (startNode) => {
  const deps = []
  const branch = []
  depth({
    tree: startNode,
    getChildren: node => node.dependencies,
    visit: node => {
      branch.push(`${node.packageName}@${node.version}`)
      deps.push(`${branch.join('->')}::${node.resolved}`)
    },
    leave: () => {
      branch.pop()
    },
  })
  deps.sort()
  // TODO these replaces were originally to deal with node 14 not supporting base64url and likely don't need to happen anymore
  // Changing this is a pretty significant breaking change, but removing parts of the hash increases collision possibilities (even if slight).
  const hash = crypto.createHash('shake256', { outputLength: 16 })
    .update(deps.join(','))
    .digest('base64')
    .replace(/\+/g, '-')
    .replace(/\//g, '_')
    .replace(/=+$/m, '')
  return `${startNode.packageName}@${startNode.version}-${hash}`
}

module.exports = cls => class IsolatedReifier extends cls {
  #externalProxies = new Map()
  #omit = new Set()
  #rootDeclaredDeps = new Set()
  #processedEdges = new Set()
  #workspaceProxies = new Map()

  #generateChild (node, location, pkg, isInStore, root) {
    const newChild = new IsolatedNode({
      isInStore,
      location,
      name: node.packageName || node.name,
      optional: node.optional,
      package: pkg,
      parent: root,
      path: join(this.idealGraph.localPath, location),
      resolved: node.resolved,
      root,
    })
    // XXX top is from place-dep not lib/node.js
    newChild.top = { path: this.idealGraph.localPath }
    root.children.set(newChild.location, newChild)
    root.inventory.set(newChild.location, newChild)
  }

  /**
   * Create an ideal graph.
   *
   * An implementation of npm RFC-0042
   * https://github.com/npm/rfcs/blob/main/accepted/0042-isolated-mode.md
   *
   * This entire file should be considered technical debt that will be resolved with an Arborist refactor or rewrite.
   * Embedded logic in Nodes and Links, and the incremental state of building trees and reifying contains too many assumptions to do a linked mode properly.
   *
   * Instead, this approach takes a tree built from build-ideal-tree, and returns a new tree-like structure without the embedded logic of Node and Link classes.
   *
   * Since the RFC requires leaving the package-lock in place, this approach temporarily replaces the tree state for a couple of steps of reifying.
   *
   **/
  async makeIdealGraph () {
    const idealTree = this.idealTree
    this.#omit = new Set(this.options.omit)
    const omit = this.#omit

    // npm auto-creates 'workspace' edges from root to all workspaces.
    // For isolated/linked mode, only include workspaces that root explicitly declares as dependencies.
    // When omitting dep types, exclude those from the declared set so their workspaces aren't hoisted.
    const rootPkg = idealTree.package
    this.#rootDeclaredDeps = new Set(Object.keys(Object.assign({},
      rootPkg.dependencies,
      (!omit.has('dev') && rootPkg.devDependencies),
      (!omit.has('optional') && rootPkg.optionalDependencies),
      (!omit.has('peer') && rootPkg.peerDependencies)
    )))

    // XXX this sometimes acts like a node too
    this.idealGraph = {
      external: [],
      isProjectRoot: true,
      localLocation: idealTree.location,
      localPath: idealTree.path,
      path: idealTree.path,
    }
    this.counter = 0

    this.idealGraph.workspaces = await Promise.all(Array.from(idealTree.fsChildren.values(), w => this.#workspaceProxy(w)))
    const processed = new Set()
    const queue = [idealTree, ...idealTree.fsChildren]
    while (queue.length !== 0) {
      const next = queue.pop()
      if (processed.has(next.location)) {
        continue
      }
      processed.add(next.location)
      next.edgesOut.forEach(edge => {
        if (edge.to && !(next.package.bundleDependencies || next.package.bundledDependencies || []).includes(edge.to.name) && !edge.to.shouldOmit?.(omit)) {
          queue.push(edge.to)
        }
      })
      // local `file:` deps are in fsChildren but are not workspaces.
      // they are already handled as workspace-like proxies above and should not go through the external/store extraction path.
      // Links with file: resolved paths (from `npm link`) should also be treated as local dependencies and symlinked directly instead of being extracted into the store.
      const isLocalFileDep = next.isLink && next.resolved?.startsWith('file:')
      if (isLocalFileDep && !idealTree.fsChildren.has(next) && !idealTree.fsChildren.has(next.target)) {
        this.idealGraph.workspaces.push(await this.#workspaceProxy(next.target))
      } else if (!next.isProjectRoot && !next.isWorkspace && !next.inert && !idealTree.fsChildren.has(next) && !idealTree.fsChildren.has(next.target)) {
        this.idealGraph.external.push(await this.#externalProxy(next))
      }
    }

    await this.#assignCommonProperties(idealTree, this.idealGraph)
  }

  async #workspaceProxy (node) {
    if (this.#workspaceProxies.has(node)) {
      return this.#workspaceProxies.get(node)
    }
    const result = {}
    // XXX this goes recursive if we don't set here because assignCommonProperties also calls this.#workspaceProxy
    this.#workspaceProxies.set(node, result)
    result.localLocation = node.location
    result.localPath = node.path
    result.isWorkspace = true
    result.resolved = node.resolved
    await this.#assignCommonProperties(node, result)
    return result
  }

  async #externalProxy (node) {
    if (this.#externalProxies.has(node)) {
      return this.#externalProxies.get(node)
    }
    const result = {}
    // XXX this goes recursive if we don't set here because assignCommonProperties also calls this.#externalProxy
    this.#externalProxies.set(node, result)
    await this.#assignCommonProperties(node, result, !node.hasShrinkwrap)
    if (node.hasShrinkwrap) {
      const dir = join(
        node.root.path,
        'node_modules',
        '.store',
        `${node.packageName}@${node.version}`
      )
      mkdirSync(dir, { recursive: true })
      // TODO this approach feels wrong and shouldn't be necessary for shrinkwraps
      await pacote.extract(node.resolved, dir, {
        ...this.options,
        resolved: node.resolved,
        integrity: node.integrity,
        // TODO _isRoot
      })
      const Arborist = this.constructor
      const arb = new Arborist({ ...this.options, path: dir })
      // Make sure that the ideal tree is build as the rest of the algorithm depends on it.
      await arb.buildIdealTree({
        complete: false,
        dev: false,
      })
      await arb.makeIdealGraph()
      this.idealGraph.external.push(...arb.idealGraph.external)
      for (const edge of arb.idealGraph.external) {
        edge.root = this.idealGraph
        edge.id = `${node.id}=>${edge.id}`
      }
      result.localDependencies = []
      result.externalDependencies = arb.idealGraph.externalDependencies
      result.externalOptionalDependencies = arb.idealGraph.externalOptionalDependencies
      result.dependencies = [
        ...result.externalDependencies,
        ...result.externalOptionalDependencies,
      ]
    }
    result.optional = node.optional
    result.resolved = node.resolved
    result.version = node.version
    return result
  }

  async #assignCommonProperties (node, result, populateDeps = true) {
    result.root = this.idealGraph
    // XXX does anything need this?
    result.id = this.counter++
    /* istanbul ignore next - packageName is always set for real packages */
    result.name = result.isWorkspace ? (node.packageName || node.name) : node.name
    result.packageName = node.packageName || node.name
    result.package = { ...node.package }
    result.package.bundleDependencies = undefined

    if (!populateDeps) {
      return
    }

    let edges = [...node.edgesOut.values()].filter(edge =>
      edge.to?.target &&
      !(node.package.bundledDependencies || node.package.bundleDependencies)?.includes(edge.to.name)
    )

    // Only omit edge types for root and workspace nodes (matching shouldOmit scope)
    if ((node.isProjectRoot || node.isWorkspace) && this.#omit.size) {
      edges = edges.filter(edge => {
        if (edge.dev && this.#omit.has('dev')) {
          return false
        }
        if (edge.optional && this.#omit.has('optional')) {
          return false
        }
        if (edge.peer && this.#omit.has('peer')) {
          return false
        }
        return true
      })
    }

    let nonOptionalDeps = edges.filter(edge => !edge.optional).map(edge => edge.to.target)

    // npm auto-creates 'workspace' edges from root to all workspaces.
    // For isolated/linked mode, only include workspaces that root explicitly declares as dependencies.
    if (node.isProjectRoot) {
      nonOptionalDeps = nonOptionalDeps.filter(n => !n.isWorkspace || this.#rootDeclaredDeps.has(n.packageName))
    }

    // When legacyPeerDeps is enabled, peer dep edges are not created on the node.
    // Resolve them from the tree so they get symlinked in the store.
    const peerDeps = node.package.peerDependencies
    if (peerDeps && node.legacyPeerDeps) {
      const edgeNames = new Set(edges.map(edge => edge.name))
      for (const peerName in peerDeps) {
        if (!edgeNames.has(peerName)) {
          const resolved = node.resolve(peerName)
          if (resolved && resolved !== node && !resolved.inert) {
            nonOptionalDeps.push(resolved.target)
          }
        }
      }
    }

    // local `file:` deps (non-workspace fsChildren) should be treated as local dependencies, not external, so they get symlinked directly instead of being extracted into the store.
    const isLocal = (n) => n.isWorkspace || node.fsChildren?.has(n)
    const optionalDeps = edges.filter(edge => edge.optional).map(edge => edge.to.target)
    result.localDependencies = await Promise.all(nonOptionalDeps.filter(isLocal).map(n => this.#workspaceProxy(n)))
    result.externalDependencies = await Promise.all(nonOptionalDeps.filter(n => !isLocal(n) && !n.inert).map(n => this.#externalProxy(n)))
    result.externalOptionalDependencies = await Promise.all(optionalDeps.filter(n => !n.inert).map(n => this.#externalProxy(n)))
    result.dependencies = [
      ...result.externalDependencies,
      ...result.localDependencies,
      ...result.externalOptionalDependencies,
    ]
  }

  async #createBundledTree () {
    // TODO: make sure that idealTree object exists
    const idealTree = this.idealTree
    // TODO: test workspaces having bundled deps
    const queue = []

    for (const [, edge] of idealTree.edgesOut) {
      if (edge.to && (idealTree.package.bundleDependencies || idealTree.package.bundledDependencies || []).includes(edge.to.name)) {
        queue.push({ from: idealTree, to: edge.to })
      }
    }
    for (const child of idealTree.fsChildren) {
      for (const [, edge] of child.edgesOut) {
        if (edge.to && (child.package.bundleDependencies || child.package.bundledDependencies || []).includes(edge.to.name)) {
          queue.push({ from: child, to: edge.to })
        }
      }
    }

    const processed = new Set()
    const nodes = new Map()
    const edges = []
    while (queue.length !== 0) {
      const nextEdge = queue.pop()
      const key = `${nextEdge.from.location}=>${nextEdge.to.location}`
      // should be impossible, unless bundled is duped
      /* istanbul ignore next */
      if (processed.has(key)) {
        continue
      }
      processed.add(key)
      const from = nextEdge.from
      if (!from.isRoot && !from.isWorkspace) {
        nodes.set(from.location, { location: from.location, resolved: from.resolved, name: from.name, optional: from.optional, pkg: { ...from.package, bundleDependencies: undefined } })
      }
      const to = nextEdge.to
      nodes.set(to.location, { location: to.location, resolved: to.resolved, name: to.name, optional: to.optional, pkg: { ...to.package, bundleDependencies: undefined } })
      edges.push({ from: from.isRoot ? 'root' : from.location, to: to.location })

      to.edgesOut.forEach(edge => {
        // an edge out should always have a to
        /* istanbul ignore else */
        if (edge.to) {
          queue.push({ from: edge.from, to: edge.to })
        }
      })
    }
    return { edges, nodes }
  }

  async createIsolatedTree () {
    await this.makeIdealGraph()
    const bundledTree = await this.#createBundledTree()

    const root = new IsolatedNode(this.idealGraph)
    root.root = root
    root.inventory.set('', root)
    const processed = new Set()
    for (const c of this.idealGraph.workspaces) {
      const wsName = c.packageName
      // XXX parent? root?
      const workspace = new IsolatedNode({
        location: c.localLocation,
        name: wsName,
        package: c.package,
        path: c.localPath,
        resolved: c.resolved,
      })
      root.fsChildren.add(workspace)
      root.inventory.set(workspace.location, workspace)
      root.workspaces.set(wsName, workspace.path)

      // Create workspace Link. For root declared deps, link at root node_modules/. For undeclared deps, link at the workspace's own node_modules/ (self-link).
      const isDeclared = this.#rootDeclaredDeps.has(wsName)
      const wsLink = new IsolatedLink({
        location: isDeclared ? join('node_modules', wsName) : join(c.localLocation, 'node_modules', wsName),
        name: wsName,
        package: workspace.package,
        parent: root,
        path: isDeclared ? join(root.path, 'node_modules', wsName) : join(root.path, c.localLocation, 'node_modules', wsName),
        realpath: workspace.path,
        root,
        target: workspace,
      })
      if (!isDeclared) {
        workspace.children.set(wsName, wsLink)
      }
      root.children.set(wsName, wsLink)
      root.inventory.set(wsLink.location, wsLink)
      workspace.linksIn.add(wsLink)
    }

    this.idealGraph.external.forEach(c => {
      const key = getKey(c)
      if (processed.has(key)) {
        return
      }
      processed.add(key)
      const location = join('node_modules', '.store', key, 'node_modules', c.packageName)
      this.#generateChild(c, location, c.package, true, root)
    })

    bundledTree.nodes.forEach(node => {
      this.#generateChild(node, node.location, node.pkg, false, root)
    })

    bundledTree.edges.forEach(edge => {
      const from = edge.from === 'root' ? root : root.inventory.get(edge.from)
      const to = root.inventory.get(edge.to)
      // Maybe optional should be propagated from the original edge
      const newEdge = { optional: false, from, to }
      from.edgesOut.set(to.name, newEdge)
      to.edgesIn.add(newEdge)
    })

    this.#processEdges(this.idealGraph, false, root)
    for (const node of this.idealGraph.workspaces) {
      this.#processEdges(node, false, root)
    }
    return root
  }

  #processEdges (node, externalEdge, root) {
    const key = getKey(node)
    if (this.#processedEdges.has(key)) {
      return
    }
    this.#processedEdges.add(key)

    let from, nmFolder
    if (externalEdge) {
      const fromLocation = join('node_modules', '.store', key, 'node_modules', node.packageName)
      from = root.children.get(fromLocation)
      nmFolder = join('node_modules', '.store', key, 'node_modules')
    } else {
      from = node.isProjectRoot ? root : root.inventory.get(node.localLocation)
      nmFolder = join(node.localLocation, 'node_modules')
    }
    /* istanbul ignore next - strict-peer-deps can exclude nodes from the tree */
    if (!from) {
      return
    }

    for (const dep of node.localDependencies) {
      this.#processEdges(dep, false, root)
      // nonOptional, local
      this.#processDeps(dep, false, false, root, from, nmFolder)
    }
    for (const dep of node.externalDependencies) {
      this.#processEdges(dep, true, root)
      // nonOptional, external
      this.#processDeps(dep, false, true, root, from, nmFolder)
    }
    for (const dep of node.externalOptionalDependencies) {
      this.#processEdges(dep, true, root)
      // optional, external
      this.#processDeps(dep, true, true, root, from, nmFolder)
    }
  }

  #processDeps (dep, optional, external, root, from, nmFolder) {
    const toKey = getKey(dep)

    let target
    if (external) {
      const toLocation = join('node_modules', '.store', toKey, 'node_modules', dep.packageName)
      target = root.children.get(toLocation)
    } else {
      target = root.inventory.get(dep.localLocation)
    }
    // TODO: we should no-op is an edge has already been created with the same fromKey and toKey
    /* istanbul ignore next - strict-peer-deps can exclude nodes from the tree */
    if (!target) {
      return
    }

    if (dep.package.bin) {
      for (const bn in dep.package.bin) {
        target.binPaths.push(join(dep.root.localPath, nmFolder, '.bin', bn))
      }
    }

    const pkg = {
      _id: dep.package._id,
      bin: target.package.bin,
      bundleDependencies: undefined,
      deprecated: undefined,
      scripts: dep.package.scripts,
      version: dep.package.version,
    }
    const link = new IsolatedLink({
      isStoreLink: true,
      location: join(nmFolder, dep.name),
      name: toKey,
      optional,
      parent: root,
      package: pkg,
      path: join(dep.root.localPath, nmFolder, dep.name),
      realpath: target.path,
      resolved: external ? `file:.store/${toKey}/node_modules/${dep.packageName}` : dep.resolved,
      root,
      target,
    })
    // XXX top is from place-dep not lib/link.js
    link.top = { path: dep.root.localPath }
    const newEdge1 = { optional, from, to: link }
    from.edgesOut.set(dep.name, newEdge1)
    link.edgesIn.add(newEdge1)
    const newEdge2 = { optional: false, from: link, to: target }
    link.edgesOut.set(dep.name, newEdge2)
    target.edgesIn.add(newEdge2)
    root.children.set(link.location, link)
  }
}
