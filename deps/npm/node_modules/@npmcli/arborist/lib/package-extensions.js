// Root-owned `packageExtensions`: declarative repairs to third-party manifests applied before Arborist reads a candidate's dependency edges.
// See RFC: https://github.com/npm/rfcs/pull/889
// This module is pure: it parses and validates the root rule set, matches a candidate manifest by name and version, and returns an extended manifest copy plus minimal provenance.
// It never mutates the input manifest or any shared cache object.
const semver = require('semver')
const ssri = require('ssri')
const validateName = require('validate-npm-package-name')

// The only manifest fields a package extension may add or correct, because they are the fields that affect dependency and peer resolution.
const EXTENSION_FIELDS = [
  'dependencies',
  'optionalDependencies',
  'peerDependencies',
  'peerDependenciesMeta',
]

// The two normal dependency fields; a name may exist in only one of them.
const NORMAL_DEP_FIELDS = ['dependencies', 'optionalDependencies']

const err = (message, code, extra = {}) =>
  Object.assign(new Error(message), { code, ...extra })

// Parse a selector key into { name, range }, where range is null for a name-only key.
// Selectors are a package name with an optional semver range; dist-tags, git, file, directory, url, and alias specs are rejected.
const parseSelector = key => {
  if (typeof key !== 'string' || !key) {
    throw err(`Invalid packageExtensions selector: ${JSON.stringify(key)}`, 'EEXTENSIONSELECTOR')
  }
  // The separator @ is the first @ after a leading scope @.
  const at = key.indexOf('@', key.startsWith('@') ? 1 : 0)
  const name = at === -1 ? key : key.slice(0, at)
  const range = at === -1 ? null : key.slice(at + 1)

  const { validForOldPackages, validForNewPackages } = validateName(name)
  if (!validForOldPackages && !validForNewPackages) {
    throw err(`Invalid package name in packageExtensions selector: "${key}"`, 'EEXTENSIONSELECTOR', { selector: key })
  }
  // A blank range such as "foo@" is malformed; the name-only form "foo" is how you match every version.
  if (range !== null && range.trim() === '') {
    throw err(
      `Invalid packageExtensions selector: "${key}". Use the name only to match every version.`,
      'EEXTENSIONSELECTOR', { selector: key })
  }
  // A versioned selector must be a valid semver range, which rejects dist-tags, git, file, url, and alias specs.
  if (range !== null && semver.validRange(range, { loose: true }) === null) {
    throw err(
      `Invalid version range in packageExtensions selector: "${key}". Selectors accept a package name with an optional semver range only.`,
      'EEXTENSIONSELECTOR', { selector: key })
  }
  return { name, range }
}

// A selector matches a candidate manifest by its own name and version.
// Name-only selectors match every version, including non-semver versions.
// Versioned selectors only match versions that parse as semver and satisfy the range.
const rangeMatches = (range, version) => {
  if (range === null) {
    return true
  }
  return semver.valid(version, { loose: true }) !== null &&
    semver.satisfies(version, range, { loose: true })
}

// Validate a single selector's extension object before it is ever applied.
const validateExtensionObject = (key, ext) => {
  if (ext === null || typeof ext !== 'object' || Array.isArray(ext)) {
    throw err(`packageExtensions["${key}"] must be an object`, 'EEXTENSIONVALUE', { selector: key })
  }
  for (const field of Object.keys(ext)) {
    if (!EXTENSION_FIELDS.includes(field)) {
      throw err(
        `packageExtensions["${key}"] has unsupported field "${field}". Supported fields: ${EXTENSION_FIELDS.join(', ')}.`,
        'EEXTENSIONFIELD', { selector: key, field })
    }
    const val = ext[field]
    if (val === null || typeof val !== 'object' || Array.isArray(val)) {
      throw err(`packageExtensions["${key}"].${field} must be an object`, 'EEXTENSIONVALUE', { selector: key, field })
    }
  }
  // Deletion is not supported in v1, so a null, false, or "-" value is an error.
  for (const field of [...NORMAL_DEP_FIELDS, 'peerDependencies']) {
    for (const [name, spec] of Object.entries(ext[field] || {})) {
      if (spec === null || spec === false || spec === '-') {
        throw err(
          `packageExtensions["${key}"].${field}.${name} attempts deletion, which is not supported.`,
          'EEXTENSIONDELETE', { selector: key, field, name })
      }
    }
  }
  // Each peerDependenciesMeta entry must be a non-null metadata object, never a deletion sentinel or primitive.
  for (const [name, meta] of Object.entries(ext.peerDependenciesMeta || {})) {
    if (meta === null || typeof meta !== 'object' || Array.isArray(meta)) {
      throw err(
        `packageExtensions["${key}"].peerDependenciesMeta.${name} must be an object`,
        'EEXTENSIONVALUE', { selector: key, field: 'peerDependenciesMeta', name })
    }
  }
}

// Apply a matched extension to a manifest, returning { pkg, applied } where pkg is a copy with extended fields and applied is minimal provenance.
// The input manifest is never mutated.
const applyExtension = (pkg, { key, ext }) => {
  const applied = { selector: key }

  // Clone only the fields we may touch; the rest of the manifest is shared by reference since it is never mutated.
  const next = { ...pkg }
  for (const field of EXTENSION_FIELDS) {
    if (pkg[field] && typeof pkg[field] === 'object') {
      next[field] = field === 'peerDependenciesMeta'
        ? Object.fromEntries(Object.entries(pkg[field]).map(([n, m]) => [n, { ...m }]))
        : { ...pkg[field] }
    }
  }

  // dependencies and optionalDependencies add missing names only.
  // A name already declared in either normal dependency field is an error, which also prevents moving a name between the fields.
  for (const field of NORMAL_DEP_FIELDS) {
    const adds = ext[field]
    if (!adds) {
      continue
    }
    for (const [name, spec] of Object.entries(adds)) {
      for (const existingField of NORMAL_DEP_FIELDS) {
        if (next[existingField] && name in next[existingField]) {
          throw err(
            `packageExtensions["${key}"].${field}.${name} conflicts with the package's existing ${existingField}.${name}. Use overrides to change a dependency version; packageExtensions only adds missing dependencies.`,
            'EEXTENSIONDUPDEP', { selector: key, field, name, existingField })
        }
      }
      next[field] = next[field] || {}
      next[field][name] = spec
      ;(applied[field] = applied[field] || []).push(name)
    }
  }

  // peerDependencies shallow-merges by peer name, and the extension value replaces an existing range.
  if (ext.peerDependencies) {
    next.peerDependencies = next.peerDependencies || {}
    for (const [name, spec] of Object.entries(ext.peerDependencies)) {
      next.peerDependencies[name] = spec
      ;(applied.peerDependencies = applied.peerDependencies || []).push(name)
    }
  }

  // peerDependenciesMeta merges by peer name, then shallow-merges each meta object so an extension can add optional without dropping other meta keys.
  if (ext.peerDependenciesMeta) {
    next.peerDependenciesMeta = next.peerDependenciesMeta || {}
    for (const [name, meta] of Object.entries(ext.peerDependenciesMeta)) {
      next.peerDependenciesMeta[name] = { ...next.peerDependenciesMeta[name], ...meta }
      ;(applied.peerDependenciesMeta = applied.peerDependenciesMeta || []).push(name)
      // Every peerDependenciesMeta entry an extension adds must correspond to a peerDependencies entry present after extension application.
      if (!next.peerDependencies || !(name in next.peerDependencies)) {
        throw err(
          `packageExtensions["${key}"].peerDependenciesMeta.${name} has no corresponding peerDependencies.${name} after extension application.`,
          'EEXTENSIONORPHANMETA', { selector: key, name })
      }
    }
  }

  return { pkg: next, applied }
}

// Deterministic JSON for hashing: keys sorted lexicographically at every level, string and number values preserved exactly, no insignificant whitespace.
const canonicalStringify = val => {
  if (Array.isArray(val)) {
    return `[${val.map(canonicalStringify).join(',')}]`
  }
  if (val && typeof val === 'object') {
    return `{${Object.keys(val).sort()
      .map(k => `${JSON.stringify(k)}:${canonicalStringify(val[k])}`)
      .join(',')}}`
  }
  return JSON.stringify(val)
}

// Hash the canonical form of the root packageExtensions object using npm's existing lockfile digest encoding.
const canonicalHash = packageExtensions =>
  ssri.fromData(canonicalStringify(packageExtensions), { algorithms: ['sha512'] }).toString()

class PackageExtensions {
  constructor (raw) {
    this.raw = raw
    this.present = raw !== undefined
    this.selectors = []
    this.hash = null

    if (!this.present) {
      return
    }
    if (raw === null || typeof raw !== 'object' || Array.isArray(raw)) {
      throw err('packageExtensions must be an object', 'EEXTENSIONROOT')
    }
    for (const [key, ext] of Object.entries(raw)) {
      const { name, range } = parseSelector(key)
      validateExtensionObject(key, ext)
      this.selectors.push({ key, name, range, ext })
    }
    this.hash = canonicalHash(raw)
  }

  // Non-throwing check used for warnings: whether any selector matches the candidate.
  wouldMatch (name, version) {
    return this.selectors.some(s => s.name === name && rangeMatches(s.range, version))
  }

  // Return the single selector matching a candidate manifest, or null.
  // Throws EEXTENSIONCONFLICT when more than one selector matches the same candidate.
  match (name, version) {
    const matches = this.selectors.filter(s => s.name === name && rangeMatches(s.range, version))
    if (matches.length > 1) {
      const keys = matches.map(s => `"${s.key}"`).join(', ')
      throw err(
        `Multiple packageExtensions selectors match ${name}@${version}: ${keys}. Narrow or remove one of the overlapping rules.`,
        'EEXTENSIONCONFLICT', { name, version, selectors: matches.map(s => s.key) })
    }
    return matches[0] || null
  }

  // Apply the matching extension to a manifest copy, returning { pkg, applied } or null when no selector matches.
  // Throws on selector conflict or invalid merge.
  apply (pkg) {
    if (!this.present || !this.selectors.length || !pkg || !pkg.name) {
      return null
    }
    const sel = this.match(pkg.name, pkg.version)
    return sel ? applyExtension(pkg, sel) : null
  }
}

module.exports = PackageExtensions
module.exports.PackageExtensions = PackageExtensions
module.exports.parseSelector = parseSelector
module.exports.rangeMatches = rangeMatches
module.exports.canonicalHash = canonicalHash
module.exports.canonicalStringify = canonicalStringify
module.exports.EXTENSION_FIELDS = EXTENSION_FIELDS
