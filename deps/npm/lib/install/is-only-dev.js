'use strict'
module.exports = isOnlyDev

const moduleName = require('../utils/module-name.js')
const isDevDep = require('./is-dev-dep.js')
const isProdDep = require('./is-prod-dep.js')

// Returns true if the module `node` is only required direcctly as a dev
// dependency of the top level or transitively _from_ top level dev
// dependencies.
// Dual mode modules (that are both dev AND prod) should return false.
function isOnlyDev (node, seen) {
  if (!seen) seen = new Set()
  return node.requiredBy.length && node.requiredBy.every(andIsOnlyDev(moduleName(node), seen))
}

// There is a known limitation with this implementation: If a dependency is
// ONLY required by cycles that are detached from the top level then it will
// ultimately return true.
//
// This is ok though: We don't allow shrinkwraps with extraneous deps and
// these situation is caught by the extraneous checker before we get here.
function andIsOnlyDev (name, seen) {
  return function (req) {
    const isDev = isDevDep(req, name)
    const isProd = isProdDep(req, name)
    if (req.isTop) {
      return isDev && !isProd
    } else {
      if (seen.has(req)) return true
      seen.add(req)
      const result = isOnlyDev(req, seen)
      seen.delete(req)
      return result
    }
  }
}
