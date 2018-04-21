'use strict'

class FiggyPudding {
  constructor (specs, opts, providers) {
    this.__specs = specs || {}
    this.__opts = opts || (() => false)
    this.__providers = reverse((providers || []).filter(
      x => x != null && typeof x === 'object'
    ))
    this.__isFiggyPudding = true
  }
  get (key) {
    return pudGet(this, key, true)
  }
  concat (...moreConfig) {
    return new FiggyPudding(
      this.__specs,
      this.__opts,
      reverse(this.__providers).concat(moreConfig)
    )
  }
}

function pudGet (pud, key, validate) {
  let spec = pud.__specs[key]
  if (typeof spec === 'string') {
    key = spec
    spec = pud.__specs[key]
  }
  if (validate && !spec && (!pud.__opts.other || !pud.__opts.other(key))) {
    throw new Error(`invalid config key requested: ${key}`)
  } else {
    if (!spec) { spec = {} }
    let ret
    for (let p of pud.__providers) {
      if (p.__isFiggyPudding) {
        ret = pudGet(p, key, false)
      } else if (typeof p.get === 'function') {
        ret = p.get(key)
      } else {
        ret = p[key]
      }
      if (ret !== undefined) {
        break
      }
    }
    if (ret === undefined && spec.default !== undefined) {
      if (typeof spec.default === 'function') {
        return spec.default()
      } else {
        return spec.default
      }
    } else {
      return ret
    }
  }
}

const proxyHandler = {
  has (obj, prop) {
    return pudGet(obj, prop, false) !== undefined
  },
  get (obj, prop) {
    if (
      prop === 'concat' ||
      prop === 'get' ||
      prop.slice(0, 2) === '__'
    ) {
      return obj[prop]
    }
    return obj.get(prop)
  },
  set (obj, prop, value) {
    if (prop.slice(0, 2) === '__') {
      obj[prop] = value
    } else {
      throw new Error('figgyPudding options cannot be modified. Use .concat() instead.')
    }
  },
  delete () {
    throw new Error('figgyPudding options cannot be deleted. Use .concat() and shadow them instead.')
  }
}

module.exports = figgyPudding
function figgyPudding (specs, opts) {
  function factory (...providers) {
    return new Proxy(new FiggyPudding(
      specs,
      opts,
      providers
    ), proxyHandler)
  }
  return factory
}

function reverse (arr) {
  const ret = []
  arr.forEach(x => ret.unshift(x))
  return ret
}
