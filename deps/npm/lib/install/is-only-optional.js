'use strict'
module.exports = isOptional

const isOptDep = require('./is-opt-dep.js')
const moduleName = require('../utils/module-name.js')

function isOptional (node, seen) {
  if (!seen) seen = new Set()
  // If a node is not required by anything, then we've reached
  // the top level package.
  if (seen.has(node) || node.requiredBy.length === 0) {
    return false
  }
  seen = new Set(seen)
  seen.add(node)
  const swOptional = node.fromShrinkwrap && node.package._optional
  return node.requiredBy.every(function (req) {
    if (req.fakeChild && swOptional) return true
    return isOptDep(req, moduleName(node)) || isOptional(req, seen)
  })
}
