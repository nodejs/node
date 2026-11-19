const npa = require('npm-package-arg')

const carriedNames = new WeakMap()

// Scoped registry credentials must follow dependency specs, not package metadata.
const getRegistryPackageName = node => {
  if (carriedNames.has(node)) {
    return carriedNames.get(node)
  }
  if (!node.edgesIn || typeof node.edgesIn[Symbol.iterator] !== 'function') {
    return null
  }

  const names = new Set()
  const peerNames = new Set()
  const rootNames = new Set()
  const aliases = new Set()
  for (const edge of node.edgesIn) {
    if (!edge.valid) {
      continue
    }
    let spec
    try {
      // Match v10 Edge.satisfiedBy: overrides do not apply inside bundles or shrinkwraps.
      const requested = node.hasShrinkwrap || node.inShrinkwrap || node.inBundle
        ? edge.rawSpec : edge.spec
      spec = npa.resolve(edge.name, requested)
    } catch {
      return null
    }
    if (!spec.registry) {
      return null
    }
    const name = spec.type === 'alias' ? spec.subSpec.name : spec.name
    if (!name) {
      return null
    }
    if (spec.type === 'alias') {
      aliases.add(name)
    }
    // Ordinary peers constrain the installed slot, not an alias's target.
    if (edge.peer && spec.type !== 'alias') {
      peerNames.add(name)
    } else {
      names.add(name)
      if (edge.from?.isProjectRoot || edge.from?.isWorkspace) {
        rootNames.add(name)
      }
    }
  }

  const identities = rootNames.size ? rootNames : names.size ? names : peerNames
  if (identities.size !== 1) {
    return null
  }
  const name = identities.values().next().value
  return [...aliases].every(alias => alias === name) ? name : null
}

const carryRegistryPackageName = (from, to) => {
  carriedNames.set(to, getRegistryPackageName(from))
}

module.exports = { carryRegistryPackageName, getRegistryPackageName }
