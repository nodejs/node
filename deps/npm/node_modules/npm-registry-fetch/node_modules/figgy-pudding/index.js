'use strict'

class FiggyPudding {
  constructor (specs, opts, providers) {
    this.specs = specs || {}
    this.opts = opts || (() => false)
    this.providers = providers
    this.isFiggyPudding = true
  }
  get (key) {
    return pudGet(this, key, true)
  }
}

function pudGet (pud, key, validate) {
  let spec = pud.specs[key]
  if (typeof spec === 'string') {
    key = spec
    spec = pud.specs[key]
  }
  if (validate && !spec && (!pud.opts.other || !pud.opts.other(key))) {
    throw new Error(`invalid config key requested: ${key}`)
  } else {
    if (!spec) { spec = {} }
    let ret
    for (let p of pud.providers) {
      if (p.isFiggyPudding) {
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

module.exports = figgyPudding
function figgyPudding (specs, opts) {
  function factory () {
    return new FiggyPudding(
      specs,
      opts,
      [].slice.call(arguments).filter(x => x != null && typeof x === 'object')
    )
  }
  return factory
}
