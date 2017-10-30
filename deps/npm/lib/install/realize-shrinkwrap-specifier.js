'use strict'
var npa = require('npm-package-arg')

module.exports = function (name, sw, where) {
  try {
    if (sw.version && sw.integrity) {
      return npa.resolve(name, sw.version, where)
    } else if (sw.from) {
      const spec = npa(sw.from, where)
      if (spec.registry && sw.version) {
        return npa.resolve(name, sw.version, where)
      } else if (!sw.resolved) {
        return spec
      }
    }
    if (sw.resolved) {
      return npa.resolve(name, sw.resolved, where)
    }
  } catch (_) { }
  return npa.resolve(name, sw.version, where)
}
