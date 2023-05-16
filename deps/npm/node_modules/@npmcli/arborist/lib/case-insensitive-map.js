// package children are represented with a Map object, but many file systems
// are case-insensitive and unicode-normalizing, so we need to treat
// node.children.get('FOO') and node.children.get('foo') as the same thing.

const _keys = Symbol('keys')
const _normKey = Symbol('normKey')
const normalize = s => s.normalize('NFKD').toLowerCase()
const OGMap = Map
module.exports = class Map extends OGMap {
  constructor (items = []) {
    super()
    this[_keys] = new OGMap()
    for (const [key, val] of items) {
      this.set(key, val)
    }
  }

  [_normKey] (key) {
    return typeof key === 'string' ? normalize(key) : key
  }

  get (key) {
    const normKey = this[_normKey](key)
    return this[_keys].has(normKey) ? super.get(this[_keys].get(normKey))
      : undefined
  }

  set (key, val) {
    const normKey = this[_normKey](key)
    if (this[_keys].has(normKey)) {
      super.delete(this[_keys].get(normKey))
    }
    this[_keys].set(normKey, key)
    return super.set(key, val)
  }

  delete (key) {
    const normKey = this[_normKey](key)
    if (this[_keys].has(normKey)) {
      const prevKey = this[_keys].get(normKey)
      this[_keys].delete(normKey)
      return super.delete(prevKey)
    }
  }

  has (key) {
    const normKey = this[_normKey](key)
    return this[_keys].has(normKey) && super.has(this[_keys].get(normKey))
  }
}
