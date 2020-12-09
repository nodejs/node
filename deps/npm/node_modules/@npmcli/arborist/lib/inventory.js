// a class to manage an inventory and set of indexes of
// a set of objects based on specific fields.
// primary is the primary index key.
// keys is the set of fields to be able to query.
const _primaryKey = Symbol('_primaryKey')
const _index = Symbol('_index')
const defaultKeys = ['name', 'license', 'funding', 'realpath']
const { hasOwnProperty } = Object.prototype
const debug = require('./debug.js')
class Inventory extends Map {
  constructor (opt = {}) {
    const { primary, keys } = opt
    super()
    this[_primaryKey] = primary || 'location'
    this[_index] = (keys || defaultKeys).reduce((index, i) => {
      index.set(i, new Map())
      return index
    }, new Map())
  }

  get primaryKey () {
    return this[_primaryKey]
  }

  get indexes () {
    return [...this[_index].keys()]
  }

  * filter (fn) {
    for (const node of this.values()) {
      if (fn(node))
        yield node
    }
  }

  add (node) {
    const root = super.get('')
    if (root && node.root !== root && node.root !== root.root) {
      debug(() => {
        throw Object.assign(new Error('adding external node to inventory'), {
          root: root.path,
          node: node.path,
          nodeRoot: node.root.path,
        })
      })
      return
    }

    const current = super.get(node[this.primaryKey])
    if (current) {
      if (current === node)
        return
      this.delete(current)
    }
    super.set(node[this.primaryKey], node)
    for (const [key, map] of this[_index].entries()) {
      // if the node has the value, but it's false, then use that
      const val_ = hasOwnProperty.call(node, key) ? node[key]
        : node[key] || (node.package && node.package[key])
      const val = typeof val_ === 'string' ? val_
        : !val_ || typeof val_ !== 'object' ? val_
        : key === 'license' ? val_.type
        : key === 'funding' ? val_.url
        : /* istanbul ignore next - not used */ val_
      const set = map.get(val) || new Set()
      set.add(node)
      map.set(val, set)
    }
  }

  delete (node) {
    if (!this.has(node))
      return

    super.delete(node[this.primaryKey])
    for (const [key, map] of this[_index].entries()) {
      const val = node[key] !== undefined ? node[key]
        : (node[key] || (node.package && node.package[key]))
      const set = map.get(val)
      if (set) {
        set.delete(node)
        if (set.size === 0)
          map.delete(node[key])
      }
    }
  }

  query (key, val) {
    const map = this[_index].get(key)
    return map && (arguments.length === 2 ? map.get(val) : map.keys()) ||
      new Set()
  }

  has (node) {
    return super.get(node[this.primaryKey]) === node
  }

  set (k, v) {
    throw new Error('direct set() not supported, use inventory.add(node)')
  }
}

module.exports = Inventory
