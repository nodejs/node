const depTypes = new Set([
  'dependencies',
  'optionalDependencies',
  'devDependencies',
  'peerDependencies',
])

// sort alphabetically all types of deps for a given package
const orderDeps = (content) => {
  for (const type of depTypes) {
    if (content && content[type]) {
      content[type] = Object.keys(content[type])
        .sort((a, b) => a.localeCompare(b, 'en'))
        .reduce((res, key) => {
          res[key] = content[type][key]
          return res
        }, {})
    }
  }
  return content
}

const updateDependencies = ({ content, originalContent }) => {
  const pkg = orderDeps({
    ...content,
  })

  // optionalDependencies don't need to be repeated in two places
  if (pkg.dependencies) {
    if (pkg.optionalDependencies) {
      for (const name of Object.keys(pkg.optionalDependencies))
        delete pkg.dependencies[name]
    }
  }

  const result = { ...originalContent }

  // loop through all types of dependencies and update package json pkg
  for (const type of depTypes) {
    if (pkg[type])
      result[type] = pkg[type]

    // prune empty type props from resulting object
    const emptyDepType =
      pkg[type]
      && typeof pkg === 'object'
      && Object.keys(pkg[type]).length === 0
    if (emptyDepType)
      delete result[type]
  }

  // if original package.json had dep in peerDeps AND deps, preserve that.
  const { dependencies: origProd, peerDependencies: origPeer } =
    originalContent || {}
  const { peerDependencies: newPeer } = result
  if (origProd && origPeer && newPeer) {
    // we have original prod/peer deps, and new peer deps
    // copy over any that were in both in the original
    for (const name of Object.keys(origPeer)) {
      if (origProd[name] !== undefined && newPeer[name] !== undefined) {
        result.dependencies = result.dependencies || {}
        result.dependencies[name] = newPeer[name]
      }
    }
  }

  return result
}

updateDependencies.knownKeys = depTypes

module.exports = updateDependencies
