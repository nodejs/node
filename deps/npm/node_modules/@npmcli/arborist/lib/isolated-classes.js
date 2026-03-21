// Alternate versions of different classes that we use for isolated mode
const CaseInsensitiveMap = require('./case-insensitive-map.js')
const { resolve } = require('node:path')

// fake lib/inventory.js
class IsolatedInventory extends Map {
  query () {
    return []
  }
}

// fake lib/node.js
class IsolatedNode {
  binPaths = []
  children = new CaseInsensitiveMap()
  edgesIn = new Set()
  edgesOut = new CaseInsensitiveMap()
  fsChildren = new Set()
  hasShrinkwrap = false
  integrity = null
  inventory = new IsolatedInventory()
  isInStore = false
  linksIn = new Set()
  meta = { loadedFromDisk: false }
  optional = false
  parent = null
  root = null
  tops = new Set()
  workspaces = new Map()

  constructor (options) {
    this.location = options.location
    this.name = options.name
    this.package = options.package
    this.path = options.path
    this.realpath = !this.isLink ? this.path : resolve(options.realpath)

    if (options.parent) {
      this.parent = options.parent
    }
    if (options.resolved) {
      this.resolved = options.resolved
    }
    if (options.root) {
      this.root = options.root
    }
    if (options.isInStore) {
      this.isInStore = true
    }
    if (options.optional) {
      this.optional = true
    }
  }

  get isRoot () {
    return this === this.root
  }

  // The idealGraph is where this is set to true
  get isProjectRoot () {
    return false
  }

  get inDepBundle () {
    return false
  }

  get isLink () {
    return false
  }

  get isTop () {
    return !this.parent
  }

  /* istanbul ignore next -- emulate lib/node.js */
  get global () {
    return false
  }

  get globalTop () {
    return false
  }

  /* istanbul ignore next -- emulate lib/node.js */
  set target (t) {
    // nop
    // In the real lib/node.js this throws in debug mode
  }

  get target () {
    return this
  }

  /* istanbul ignore next -- emulate lib/node.js */
  getBundler () {
    return null
  }

  /* istanbul ignore next -- emulate lib/node.js */
  get hasInstallScript () {
    const { hasInstallScript, scripts } = this.package
    const { install, preinstall, postinstall } = scripts || {}
    return !!(hasInstallScript || install || preinstall || postinstall)
  }

  get version () {
    return this.package.version
  }
}

// fake lib/link.js
class IsolatedLink extends IsolatedNode {
  #target
  isStoreLink = false

  constructor (options) {
    super(options)
    this.#target = options.target
    if (options.isStoreLink) {
      this.isStoreLink = true
    }
  }

  get isLink () {
    return true
  }

  set target (t) {
    this.#target = t
  }

  get target () {
    return this.#target
  }
}

module.exports = { IsolatedNode, IsolatedLink }
