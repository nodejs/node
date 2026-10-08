// Root-owned `.npm-extension.{mjs,cjs}`: an imperative `transformManifest(pkg, context)` extension point that repairs third-party manifests before Arborist reads a candidate's dependency edges.
// See RFC: https://github.com/npm/rfcs/pull/903
// This module discovers and hashes the root extension file, loads its `transformManifest` export, and applies it to a deeply isolated manifest copy, returning an extended manifest plus minimal provenance.
// It never mutates the input manifest or any shared cache object.
const { resolve, sep } = require('node:path')
const { readFileSync, existsSync } = require('node:fs')
const { pathToFileURL } = require('node:url')
const { isDeepStrictEqual } = require('node:util')
const { log } = require('proc-log')
const ssri = require('ssri')
const { EXTENSION_FIELDS } = require('./package-extensions.js')

const EXTENSION_POINT = 'transformManifest'

// The two supported module formats and their default discovery filenames.
const FORMATS = [
  { ext: 'mjs', file: '.npm-extension.mjs' },
  { ext: 'cjs', file: '.npm-extension.cjs' },
]

const err = (message, code, extra = {}) =>
  Object.assign(new Error(message), { code, ...extra })

// Read a file's bytes, or null when it does not exist.
const readBytes = path => {
  try {
    return readFileSync(path)
  } catch (e) {
    if (e.code === 'ENOENT') {
      return null
    }
    throw e
  }
}

// Resolve which extension file to load, returning { path, format, bytes } or null when none is present.
// A configured `extension-file` wins over default discovery; it must resolve inside the project root and use a `.mjs` or `.cjs` extension.
// When both default files exist npm fails rather than choosing one implicitly.
const discover = (root, extensionFile) => {
  if (extensionFile) {
    const path = resolve(root, extensionFile)
    if (path !== root && !path.startsWith(root + sep)) {
      throw err(`extension-file "${extensionFile}" must resolve inside the project root`, 'ENPMEXTENSIONPATH')
    }
    const format = FORMATS.find(f => path.endsWith(`.${f.ext}`))?.ext
    if (!format) {
      throw err(`extension-file "${extensionFile}" must use a .mjs or .cjs extension`, 'ENPMEXTENSIONPATH')
    }
    const bytes = readBytes(path)
    if (bytes === null) {
      throw err(`extension-file "${extensionFile}" was not found`, 'ENPMEXTENSIONPATH')
    }
    return { path, format, bytes }
  }
  const found = FORMATS
    .map(f => ({ path: resolve(root, f.file), format: f.ext, bytes: readBytes(resolve(root, f.file)) }))
    .filter(f => f.bytes !== null)
  if (found.length > 1) {
    throw err('found both .npm-extension.mjs and .npm-extension.cjs; keep only one', 'ENPMEXTENSIONDUP')
  }
  return found[0] || null
}

// Hash the selected extension file: a format-tagged prefix plus the raw file bytes, using npm's lockfile digest encoding.
// The prefix keeps `.mjs` and `.cjs` files with identical bytes distinct, and excludes any path so the digest is machine-independent.
const hashFile = (format, bytes) =>
  ssri.fromData(
    Buffer.concat([Buffer.from(`npm-extension:v1:${format}\n`), bytes]),
    { algorithms: ['sha512'] }
  ).toString()

class NpmExtension {
  #cache = new Map()
  #transform = null

  constructor ({ root, extensionFile = null } = {}) {
    this.root = root
    this.path = null
    this.format = null
    this.hash = null

    const selected = root ? discover(root, extensionFile) : null
    if (!selected) {
      this.present = false
      return
    }
    this.present = true
    this.path = selected.path
    this.format = selected.format
    this.hash = hashFile(selected.format, selected.bytes)
  }

  // Import the extension module and capture its `transformManifest` export.
  // ESM files are loaded with dynamic import; CommonJS files with require.
  // The export must be a function; anything else fails the install.
  async load () {
    if (!this.present) {
      return
    }
    // Key the module load by the file hash so a changed file is reloaded rather than served stale from a module cache within one process.
    let mod
    if (this.format === 'mjs') {
      mod = await import(`${pathToFileURL(this.path).href}?h=${this.hash}`)
    } else {
      delete require.cache[require.resolve(this.path)]
      mod = require(this.path)
    }
    const transform = mod?.transformManifest ?? mod?.default?.transformManifest
    if (typeof transform !== 'function') {
      throw err(`.npm-extension must export a transformManifest function`, 'ENPMEXTENSIONSHAPE')
    }
    this.#transform = transform
  }

  // Apply transformManifest to a candidate manifest, returning { pkg, applied } or null when nothing changed.
  // Results are cached once per resolved package identity; consumers get a deeply isolated copy so they cannot mutate the cached effective manifest.
  // Pass { memoize: false } to run without caching, e.g. a staleness probe over partial lockfile manifests that must not seed the cache used by full-manifest fetches.
  apply (pkg, { memoize = true } = {}) {
    if (!this.#transform || !pkg?.name) {
      return null
    }
    const key = this.#identity(pkg)
    let result
    if (this.#cache.has(key)) {
      result = this.#cache.get(key)
    } else {
      result = this.#run(pkg)
      if (memoize) {
        this.#cache.set(key, result)
      }
    }
    return result && { pkg: structuredClone(result.pkg), applied: structuredClone(result.applied) }
  }

  // Identity key for the transform cache: package integrity when available, otherwise resolved source plus name and version.
  #identity (pkg) {
    return pkg._integrity || `${pkg._resolved || ''}:${pkg.name}@${pkg.version || ''}`
  }

  // Run transformManifest on a deeply isolated copy and validate the result.
  // Returns { pkg, applied } when dependency or peer metadata changed, or null when it did not.
  #run (pkg) {
    const context = {
      log: message => log.silly('npm-extension', message),
      root: this.root,
      extensionPoint: EXTENSION_POINT,
    }
    let returned
    try {
      returned = this.#transform(structuredClone(pkg), context)
    } catch (e) {
      throw err(
        `.npm-extension transformManifest threw while processing ${pkg.name}@${pkg.version}: ${e.message}`,
        'ENPMEXTENSIONTHROW', { pkgid: `${pkg.name}@${pkg.version}` })
    }
    if (returned && typeof returned.then === 'function') {
      throw err(
        `.npm-extension transformManifest must return a manifest synchronously, not a promise, for ${pkg.name}@${pkg.version}`,
        'ENPMEXTENSIONRETURN', { pkgid: `${pkg.name}@${pkg.version}` })
    }
    if (!returned || typeof returned !== 'object' || Array.isArray(returned)) {
      throw err(
        `.npm-extension transformManifest must return a manifest object for ${pkg.name}@${pkg.version}`,
        'ENPMEXTENSIONRETURN', { pkgid: `${pkg.name}@${pkg.version}` })
    }
    // Only dependency and peer fields may change; any other field the returned manifest explicitly alters is rejected.
    // Fields the returned object omits are left untouched, so a handler may return a new object with only the fields it repairs.
    for (const k of Object.keys(returned)) {
      if (!EXTENSION_FIELDS.includes(k) && !isDeepStrictEqual(returned[k], pkg[k])) {
        throw err(
          `.npm-extension transformManifest changed unsupported field "${k}" on ${pkg.name}@${pkg.version}; only ${EXTENSION_FIELDS.join(', ')} may change`,
          'ENPMEXTENSIONFIELD', { pkgid: `${pkg.name}@${pkg.version}`, field: k })
      }
    }
    // Build the effective manifest from the normalized baseline plus the returned allowlisted fields.
    // A field the handler omits is left as the baseline; delete individual entries by returning a field object without them.
    const next = { ...pkg }
    for (const field of EXTENSION_FIELDS) {
      if (returned[field] === undefined) {
        continue
      }
      this.#validateField(field, returned[field], pkg)
      next[field] = returned[field]
    }
    const applied = this.#provenance(pkg, next)
    return applied && { pkg: next, applied }
  }

  // Validate a returned allowlisted field and its entries, so invalid output fails with .npm-extension and package context.
  // Dependency maps hold version strings; peerDependenciesMeta holds metadata objects.
  #validateField (field, value, pkg) {
    const fail = (suffix) => err(
      `.npm-extension transformManifest set ${suffix} to an invalid value on ${pkg.name}@${pkg.version}`,
      'ENPMEXTENSIONVALUE', { pkgid: `${pkg.name}@${pkg.version}`, field })
    if (value === null || typeof value !== 'object' || Array.isArray(value)) {
      throw fail(field)
    }
    for (const [name, entry] of Object.entries(value)) {
      if (field === 'peerDependenciesMeta') {
        if (entry === null || typeof entry !== 'object' || Array.isArray(entry)) {
          throw fail(`${field}.${name}`)
        }
      } else if (typeof entry !== 'string') {
        throw fail(`${field}.${name}`)
      }
    }
  }

  // Minimal provenance: the extension point plus, for each changed allowlisted field, a sorted array of affected dependency names.
  // Returns null when nothing changed.
  #provenance (before, after) {
    const applied = { extensionPoint: EXTENSION_POINT }
    let changed = false
    for (const field of EXTENSION_FIELDS) {
      const b = before[field] || {}
      const a = after[field] || {}
      const names = [...new Set([...Object.keys(b), ...Object.keys(a)])]
        .filter(n => !isDeepStrictEqual(b[n], a[n]))
        .sort()
      if (names.length) {
        applied[field] = names
        changed = true
      }
    }
    return changed ? applied : null
  }
}

// Whether a directory contains any default .npm-extension file; a non-throwing existence check used for non-root workspace warnings.
const hasExtensionFile = dir => FORMATS.some(f => existsSync(resolve(dir, f.file)))

module.exports = NpmExtension
module.exports.NpmExtension = NpmExtension
module.exports.discover = discover
module.exports.hasExtensionFile = hasExtensionFile
module.exports.hashFile = hashFile
module.exports.EXTENSION_POINT = EXTENSION_POINT
