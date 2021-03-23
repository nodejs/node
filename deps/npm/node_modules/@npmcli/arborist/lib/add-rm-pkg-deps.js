// add and remove dependency specs to/from pkg manifest

const removeFromOthers = (name, type, pkg) => {
  const others = new Set([
    'dependencies',
    'optionalDependencies',
    'devDependencies',
    'peerDependenciesMeta',
    'peerDependencies',
  ])

  switch (type) {
    case 'prod':
      others.delete('dependencies')
      break
    case 'dev':
      others.delete('devDependencies')
      others.delete('peerDependencies')
      others.delete('peerDependenciesMeta')
      break
    case 'optional':
      others.delete('optionalDependencies')
      break
    case 'peer':
    case 'peerOptional':
      others.delete('devDependencies')
      others.delete('peerDependencies')
      others.delete('peerDependenciesMeta')
      break
  }

  for (const other of others)
    deleteSubKey(pkg, other, name)
}

const add = ({pkg, add, saveBundle, saveType}) => {
  for (const spec of add)
    addSingle({pkg, spec, saveBundle, saveType})

  return pkg
}

const addSingle = ({pkg, spec, saveBundle, saveType}) => {
  if (!saveType)
    saveType = getSaveType(pkg, spec)

  const { name, rawSpec } = spec
  removeFromOthers(name, saveType, pkg)
  const type = saveType === 'prod' ? 'dependencies'
    : saveType === 'optional' ? 'optionalDependencies'
    : saveType === 'peer' || saveType === 'peerOptional' ? 'peerDependencies'
    : saveType === 'dev' ? 'devDependencies'
    : /* istanbul ignore next */ null

  pkg[type] = pkg[type] || {}
  if (rawSpec !== '' || pkg[type][name] === undefined)
    pkg[type][name] = rawSpec || '*'

  if (saveType === 'peer' || saveType === 'peerOptional') {
    const pdm = pkg.peerDependenciesMeta || {}
    if (saveType === 'peer' && pdm[name] && pdm[name].optional)
      pdm[name].optional = false
    else if (saveType === 'peerOptional') {
      pdm[name] = pdm[name] || {}
      pdm[name].optional = true
      pkg.peerDependenciesMeta = pdm
    }
    // peerDeps are often also a devDep, so that they can be tested when
    // using package managers that don't auto-install peer deps
    if (pkg.devDependencies && pkg.devDependencies[name] !== undefined)
      pkg.devDependencies[name] = pkg.peerDependencies[name]
  }

  if (saveBundle && saveType !== 'peer' && saveType !== 'peerOptional') {
    // keep it sorted, keep it unique
    const bd = new Set(pkg.bundleDependencies || [])
    bd.add(spec.name)
    pkg.bundleDependencies = [...bd].sort((a, b) => a.localeCompare(b))
  }
}

const getSaveType = (pkg, spec) => {
  const {name} = spec
  const {
    // these names are so lonnnnngggg
    devDependencies: devDeps,
    optionalDependencies: optDeps,
    peerDependencies: peerDeps,
    peerDependenciesMeta: peerDepsMeta,
  } = pkg

  if (peerDeps && peerDeps[name] !== undefined) {
    if (peerDepsMeta && peerDepsMeta[name] && peerDepsMeta[name].optional)
      return 'peerOptional'
    else
      return 'peer'
  } else if (devDeps && devDeps[name] !== undefined)
    return 'dev'
  else if (optDeps && optDeps[name] !== undefined)
    return 'optional'
  else
    return 'prod'
}

const deleteSubKey = (obj, k, sk) => {
  if (obj[k]) {
    delete obj[k][sk]
    if (!Object.keys(obj[k]).length)
      delete obj[k]
  }
}

const rm = (pkg, rm) => {
  for (const type of [
    'dependencies',
    'optionalDependencies',
    'peerDependencies',
    'peerDependenciesMeta',
    'devDependencies',
  ]) {
    for (const name of rm)
      deleteSubKey(pkg, type, name)
  }
  if (pkg.bundleDependencies) {
    pkg.bundleDependencies = pkg.bundleDependencies
      .filter(name => !rm.includes(name))
    if (!pkg.bundleDependencies.length)
      delete pkg.bundleDependencies
  }
  return pkg
}

module.exports = { add, rm }
