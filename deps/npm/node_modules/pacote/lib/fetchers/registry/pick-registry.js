'use strict'

module.exports = pickRegistry
function pickRegistry (spec, opts) {
  let registry = spec.scope && opts.scopeTargets[spec.scope]

  if (!registry && opts.scope) {
    const prefix = opts.scope[0] === '@' ? '' : '@'
    registry = opts.scopeTargets[prefix + opts.scope]
  }

  if (!registry) {
    registry = opts.registry
  }

  return registry
}
