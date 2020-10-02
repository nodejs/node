const relpath = require('./relpath.js')
const Node = require('./node.js')
const _loadDeps = Symbol.for('Arborist.Node._loadDeps')
const _target = Symbol('_target')
const {dirname} = require('path')
class Link extends Node {
  constructor (options) {
    const { realpath, target } = options

    if (!realpath && !(target && target.path))
      throw new TypeError('must provide realpath for Link node')

    super({
      ...options,
      realpath: realpath || target.path,
    })

    this.target = target || new Node({
      ...options,
      path: realpath,
      parent: null,
      root: this.root,
      linksIn: [this],
    })

    if (this.root.meta)
      this.root.meta.add(this)
  }

  get target () {
    return this[_target]
  }

  set target (target) {
    const current = this[_target]
    if (current && current.linksIn)
      current.linksIn.delete(this)

    this[_target] = target

    if (!target) {
      this.package = {}
      return
    }

    if (target.then) {
      // can set to a promise during an async tree build operation
      // wait until then to assign it.
      target.then(node => this.target = node)
      return
    }

    this.package = target.package

    this.realpath = target.path
    if (target.root === target)
      target.root = this.root
    target.linksIn.add(this)
  }

  // a link always resolves to the relative path to its target
  get resolved () {
    return this.path && this.realpath
      ? `file:${relpath(dirname(this.path), this.realpath)}`
      : null
  }

  set resolved (r) {}

  // deps are resolved on the target, not the Link
  // so this is a no-op
  [_loadDeps] () {}

  // links can't have children, only their targets can
  // fix it to an empty list so that we can still call
  // things that iterate over them, just as a no-op
  get children () {
    return new Map()
  }

  set children (c) {}

  get isLink () {
    return true
  }
}

module.exports = Link
