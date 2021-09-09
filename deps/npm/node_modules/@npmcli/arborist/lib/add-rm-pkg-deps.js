// add and remove dependency specs to/from pkg manifest

const add = ({pkg, add, saveBundle, saveType, log}) => {
  for (const spec of add) {
    addSingle({pkg, spec, saveBundle, saveType, log})
  }

  return pkg
}

// Canonical source of both the map between saveType and where it correlates to
// in the package, and the names of all our dependencies attributes
const saveTypeMap = new Map([
  ['dev', 'devDependencies'],
  ['optional', 'optionalDependencies'],
  ['prod', 'dependencies'],
  ['peerOptional', 'peerDependencies'],
  ['peer', 'peerDependencies'],
])

const addSingle = ({pkg, spec, saveBundle, saveType, log}) => {
  const { name, rawSpec } = spec

  // if the user does not give us a type, we infer which type(s)
  // to keep based on the same order of priority we do when
  // building the tree as defined in the _loadDeps method of
  // the node class.
  if (!saveType) {
    saveType = inferSaveType(pkg, spec.name)
  }

  if (saveType === 'prod') {
    // a production dependency can only exist as production (rpj ensures it
    // doesn't coexist w/ optional)
    deleteSubKey(pkg, 'devDependencies', name, 'dependencies', log)
    deleteSubKey(pkg, 'peerDependencies', name, 'dependencies', log)
  } else if (saveType === 'dev') {
    // a dev dependency may co-exist as peer, or optional, but not production
    deleteSubKey(pkg, 'dependencies', name, 'devDependencies', log)
  } else if (saveType === 'optional') {
    // an optional dependency may co-exist as dev (rpj ensures it doesn't
    // coexist w/ prod)
    deleteSubKey(pkg, 'peerDependencies', name, 'optionalDependencies', log)
  } else { // peer or peerOptional is all that's left
    // a peer dependency may coexist as dev
    deleteSubKey(pkg, 'dependencies', name, 'peerDependencies', log)
    deleteSubKey(pkg, 'optionalDependencies', name, 'peerDependencies', log)
  }

  const depType = saveTypeMap.get(saveType)

  pkg[depType] = pkg[depType] || {}
  if (rawSpec !== '' || pkg[depType][name] === undefined) {
    pkg[depType][name] = rawSpec || '*'
  }
  if (saveType === 'optional') {
    // Affordance for previous npm versions that require this behaviour
    pkg.dependencies = pkg.dependencies || {}
    pkg.dependencies[name] = pkg.optionalDependencies[name]
  }

  if (saveType === 'peer' || saveType === 'peerOptional') {
    const pdm = pkg.peerDependenciesMeta || {}
    if (saveType === 'peer' && pdm[name] && pdm[name].optional) {
      pdm[name].optional = false
    } else if (saveType === 'peerOptional') {
      pdm[name] = pdm[name] || {}
      pdm[name].optional = true
      pkg.peerDependenciesMeta = pdm
    }
    // peerDeps are often also a devDep, so that they can be tested when
    // using package managers that don't auto-install peer deps
    if (pkg.devDependencies && pkg.devDependencies[name] !== undefined) {
      pkg.devDependencies[name] = pkg.peerDependencies[name]
    }
  }

  if (saveBundle && saveType !== 'peer' && saveType !== 'peerOptional') {
    // keep it sorted, keep it unique
    const bd = new Set(pkg.bundleDependencies || [])
    bd.add(spec.name)
    pkg.bundleDependencies = [...bd].sort((a, b) => a.localeCompare(b, 'en'))
  }
}

// Finds where the package is already in the spec and infers saveType from that
const inferSaveType = (pkg, name) => {
  for (const saveType of saveTypeMap.keys()) {
    if (hasSubKey(pkg, saveTypeMap.get(saveType), name)) {
      if (
        saveType === 'peerOptional' &&
        (!hasSubKey(pkg, 'peerDependenciesMeta', name) ||
        !pkg.peerDependenciesMeta[name].optional)
      ) {
        return 'peer'
      }
      return saveType
    }
  }
  return 'prod'
}

const { hasOwnProperty } = Object.prototype
const hasSubKey = (pkg, depType, name) => {
  return pkg[depType] && hasOwnProperty.call(pkg[depType], name)
}

// Removes a subkey and warns about it if it's being replaced
const deleteSubKey = (pkg, depType, name, replacedBy, log) => {
  if (hasSubKey(pkg, depType, name)) {
    if (replacedBy && log) {
      log.warn('idealTree', `Removing ${depType}.${name} in favor of ${replacedBy}.${name}`)
    }
    delete pkg[depType][name]

    // clean up peerDepsMeta if we are removing something from peerDependencies
    if (depType === 'peerDependencies' && pkg.peerDependenciesMeta) {
      delete pkg.peerDependenciesMeta[name]
      if (!Object.keys(pkg.peerDependenciesMeta).length) {
        delete pkg.peerDependenciesMeta
      }
    }

    if (!Object.keys(pkg[depType]).length) {
      delete pkg[depType]
    }
  }
}

const rm = (pkg, rm) => {
  for (const depType of new Set(saveTypeMap.values())) {
    for (const name of rm) {
      deleteSubKey(pkg, depType, name)
    }
  }
  if (pkg.bundleDependencies) {
    pkg.bundleDependencies = pkg.bundleDependencies
      .filter(name => !rm.includes(name))
    if (!pkg.bundleDependencies.length) {
      delete pkg.bundleDependencies
    }
  }
  return pkg
}

module.exports = { add, rm, saveTypeMap, hasSubKey }
