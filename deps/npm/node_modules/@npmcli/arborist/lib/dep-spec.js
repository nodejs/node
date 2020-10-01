const types = [
  'peerDependencies',
  'devDependencies',
  'optionalDependencies',
  'dependencies',
]

const findType = (pkg, name) => {
  for (const t of types) {
    if (pkg[t] && typeof pkg[t] === 'object' && pkg[t][name] !== undefined)
      return t
  }
  return 'dependencies'
}

// given a dep name and spec, update it wherever it exists in
// the manifest, or add the spec to 'dependencies' if not found.
const updateDepSpec = (pkg, name, newSpec) => {
  const type = findType(pkg, name)
  pkg[type] = pkg[type] || {}
  pkg[type][name] = newSpec
  return pkg
}

// sort alphabetically all types of deps for a given package
const orderDeps = (pkg) => {
  for (const type of types) {
    if (pkg && pkg[type]) {
      pkg[type] = Object.keys(pkg[type])
        .sort((a, b) => a.localeCompare(b))
        .reduce((res, key) => {
          res[key] = pkg[type][key]
          return res
        }, {})
    }
  }
  return pkg
}

module.exports = {
  orderDeps,
  updateDepSpec,
}
