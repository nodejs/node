'use strict'
var npa = require('npm-package-arg')

module.exports = function (name, sw, where) {
  try {
    if (sw.version && sw.integrity) {
      return npa.resolve(name, sw.version, where)
    }
    if (sw.resolved) {
      return npa.resolve(name, sw.resolved, where)
    }
    if (sw.from) {
      var spec = npa(sw.from, where)
      if (!spec.registry) return spec
    }
  } catch (_) { }
  return npa.resolve(name, sw.version, where)
}
