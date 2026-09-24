// Resolve the root patchedDependencies map against an ideal tree.
// Attaches node.patched = { path, integrity } to each matched node.
// Enforces the failure modes (workspace-member entry, missing file, unused patch, non-registry target, ambiguous selectors) as hard errors.
const semver = require('semver')
const npa = require('npm-package-arg')
const { log } = require('proc-log')
const { resolve, relative, isAbsolute } = require('node:path')
const { readFile } = require('node:fs/promises')
const { patchIntegrity } = require('./patch.js')

// Split a selector key into { name, spec }. spec is null for a name-only key.
const parseSelector = key => {
  const at = key.indexOf('@', 1)
  return at === -1
    ? { name: key, spec: null }
    : { name: key.slice(0, at), spec: key.slice(at + 1) }
}

const err = (message, code, extra = {}) =>
  Object.assign(new Error(message), { code, ...extra })

// Pick the most specific range among several that all match a version.
// Returns the strict subset, or throws when ordering is ambiguous.
// semver.subset is transitive, so the running minimum is a subset of every range it did not throw on.
const pickRange = (ranges, name, version) => {
  let best = ranges[0]
  for (const r of ranges.slice(1)) {
    if (semver.subset(r.spec, best.spec, { loose: true })) {
      best = r
    } else if (!semver.subset(best.spec, r.spec, { loose: true })) {
      throw err(
        `Ambiguous patch selectors for ${name}@${version}: ` +
          `"${name}@${best.spec}" and "${name}@${r.spec}" overlap but neither ` +
          `is a subset. Add an exact "${name}@${version}" entry to disambiguate.`,
        'EPATCHAMBIGUOUS'
      )
    }
  }
  return best
}

// Choose the winning selector for a node: exact > range subset > name-only.
const matchSelector = (selectors, node) => {
  const { name, version } = node
  const matches = selectors.filter(s => s.name === name)
  if (!matches.length) {
    return null
  }

  const exact = matches.find(s =>
    s.spec && semver.valid(s.spec) && semver.eq(s.spec, version, { loose: true }))
  if (exact) {
    return exact
  }

  const ranges = matches.filter(s =>
    s.spec && !semver.valid(s.spec) && semver.satisfies(version, s.spec, { loose: true }))
  if (ranges.length) {
    return pickRange(ranges, name, version)
  }

  return matches.find(s => s.spec === null) || null
}

const resolvePatchedDependencies = async (tree, { path, allowUnusedPatches, rm }) => {
  const patchedDependencies = tree.package?.patchedDependencies || {}
  const selectors = Object.entries(patchedDependencies)
    .map(([key, patchPath]) => ({ ...parseSelector(key), key, patchPath }))

  // cache patch file integrity by path so shared diffs are read once
  const integrityCache = new Map()
  const readPatch = async patchPath => {
    if (integrityCache.has(patchPath)) {
      return integrityCache.get(patchPath)
    }
    // patch files must live inside the project so the patch set stays auditable
    const abs = resolve(path, patchPath)
    const rel = relative(path, abs)
    if (!rel || rel.startsWith('..') || isAbsolute(rel)) {
      throw err(`patch path escapes the project: ${patchPath}`, 'EPATCHUNSAFE', { path: patchPath })
    }
    let contents
    try {
      contents = await readFile(abs)
    } catch {
      throw err(`patch file not found: ${patchPath}`, 'EPATCHNOTFOUND', { path: patchPath })
    }
    const integrity = patchIntegrity(contents)
    integrityCache.set(patchPath, integrity)
    return integrity
  }

  const usedKeys = new Set()
  for (const node of tree.inventory.values()) {
    // patchedDependencies is honoured only in the root manifest
    if (node.isWorkspace) {
      // Link.package already delegates to its target's package
      const pkg = node.package
      if (pkg?.patchedDependencies && Object.keys(pkg.patchedDependencies).length) {
        throw err(
          `patchedDependencies is only supported in the root package.json, ` +
            `but was found in workspace "${node.name}". Move the entry to the root.`,
          'EPATCHWORKSPACE',
          { workspace: node.name }
        )
      }
      continue
    }
    if (node.isProjectRoot) {
      continue
    }

    const selector = matchSelector(selectors, node)
    if (!selector) {
      // a node that was patched but no longer matches a selector must be re-extracted to revert its files
      if (node.patched) {
        node.patchRemoved = true
      }
      node.patched = null
      continue
    }

    // a non-registry consumer edge (file:, git:, http(s)) means there is no registry tarball to patch; npm: aliases stay registry.
    // checking edges (not isRegistryDependency) avoids rejecting an edgeless node, which is still a registry dep.
    if ([...node.edgesIn].some(e => e.spec && !npa(e.spec).registry)) {
      throw err(
        `Cannot patch non-registry dependency ${node.name}@${node.version} ` +
          `(selector "${selector.key}"). Only registry dependencies can be patched.`,
        'EPATCHNONREGISTRY',
        { node: node.name }
      )
    }

    const integrity = await readPatch(selector.patchPath)
    node.patched = { path: selector.patchPath, integrity }
    usedKeys.add(selector.key)
  }

  if (selectors.length) {
    const unused = selectors.filter(s => !usedKeys.has(s.key))

    // an explicit `npm uninstall <name>` orphans that package's patch entry, so drop it instead of failing
    const removed = new Set(rm)
    const dropped = unused.filter(s => removed.has(s.name))
    if (dropped.length) {
      const patched = { ...tree.package.patchedDependencies }
      for (const s of dropped) {
        delete patched[s.key]
        log.notice('patch', `Removing patch entry "${s.key}" for uninstalled ${s.name}; left ${s.patchPath} in place.`)
      }
      // undefined drops the key entirely when reify writes package.json
      tree.package.patchedDependencies = Object.keys(patched).length ? patched : undefined
    }

    const stillUnused = unused.filter(s => !removed.has(s.name))
    if (stillUnused.length && !allowUnusedPatches) {
      throw err(
        `The following patches were registered but matched no installed ` +
          `package:\n${stillUnused.map(s => `  ${s.key} -> ${s.patchPath}`).join('\n')}\n` +
          `Use --allow-unused-patches to install anyway.`,
        'EPATCHUNUSED',
        { unused: stillUnused.map(s => s.key) }
      )
    }
  }
}

module.exports = { resolvePatchedDependencies, matchSelector, parseSelector }
