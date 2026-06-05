const npa = require('npm-package-arg')
const { log } = require('proc-log')
const { getTrustedRegistryIdentity } = require('@npmcli/arborist/lib/script-allowed.js')

// Pure helpers that implement the RFC's pin-mismatch table for
// `npm approve-scripts` and `npm deny-scripts`.
//
// Approving writes either `"<spec>": true` or `"<name>": true` to the
// project's `allowScripts` field, depending on `--allow-scripts-pin` and the currently
// installed versions.
//
// Denying always writes `"<name>": false`, regardless of `--allow-scripts-pin`, per the
// RFC's asymmetric-pin rule.

// Convert an arborist Node into the spec string used for a versioned policy
// entry. Returns `null` if the node cannot be represented as a versioned key
// derived from trusted sources (lockfile URL for registry, hosted shortcut
// for git, the resolved file path for local installs). Never falls back to
// `node.packageName` / `node.version`, which are tarball-controlled.
const versionedKeyFor = (node) => {
  if (!node) {
    return null
  }
  /* istanbul ignore next: callers guarantee a string resolved */
  const resolved = typeof node.resolved === 'string' ? node.resolved : ''
  if (resolved.startsWith('git')) {
    try {
      const parsed = npa(resolved)
      if (parsed.hosted) {
        const committish = parsed.gitCommittish || parsed.hosted.committish
        const base = parsed.hosted.shortcut({ noCommittish: true })
        return committish ? `${base}#${committish}` : base
      }
    } catch {
      /* istanbul ignore next: npa already parsed this string in keyTargetsNode */
      return null
    }
    return null
  }
  if (/^https?:\/\//.test(resolved)) {
    const trusted = getTrustedRegistryIdentity(node)
    if (trusted && trusted.version) {
      return `${trusted.name}@${trusted.version}`
    }
    // Registry node with a resolved URL that versionFromTgz couldn't
    // parse (private-registry mirror, alternate CDN URL shape). Leave a
    // breadcrumb so users notice when policy keys are silently pruned.
    log.silly(
      'allow-scripts',
      `unable to derive trusted versioned key for ${node.path || node.name || '<unknown>'} ` +
      `(resolved: ${resolved}); key will be pruned on next save`
    )
    return null
  }
  /* istanbul ignore next: 'file:' and '/' branches are each covered separately */
  if (resolved.startsWith('file:') || resolved.startsWith('/')) {
    return resolved
  }
  // No trusted source. Refuse to compose a key from attacker-controlled
  // `node.packageName` / `node.version`.
  /* istanbul ignore next: callers filter out non-registry/non-file nodes before reaching this fallback */
  return null
}

// Convert an arborist Node into the spec string used for a name-only policy
// entry. Same trust rules as versionedKeyFor — returns `null` rather than
// falling back to tarball-controlled fields.
const nameKeyFor = (node) => {
  if (!node) {
    return null
  }
  /* istanbul ignore next: callers guarantee a string resolved */
  const resolved = typeof node.resolved === 'string' ? node.resolved : ''
  if (resolved.startsWith('git')) {
    try {
      const parsed = npa(resolved)
      if (parsed.hosted) {
        return parsed.hosted.shortcut({ noCommittish: true })
      }
    } catch {
      /* istanbul ignore next: npa already parsed this string in keyTargetsNode */
      return null
    }
    return null
  }
  if (resolved.startsWith('file:') || resolved.startsWith('/')) {
    return resolved
  }
  // Registry deps: only the URL-derived (or edges-derived, in the
  // omit-lockfile case) trusted name is acceptable.
  const trusted = getTrustedRegistryIdentity(node)
  return trusted ? trusted.name : null
}

const isSingleVersionPin = (key) => {
  try {
    const parsed = npa(key)
    return parsed.type === 'version'
  } catch {
    return false
  }
}

// Build the warning string emitted when an existing deny entry blocks
// an approval. Per RFC, a name-only deny ("pkg": false) is widest and
// the only remediation is to remove the entry. A versioned deny
// ("pkg@1.2.3": false or a disjunction) blocks only specific versions;
// the user can either widen it via `npm deny-scripts <name>` or remove
// it to approve the currently-installed version only.
const denyWarning = (key, subject, name) => {
  if (isNameOnlyKey(key)) {
    return `${key} is denied; remove the entry from allowScripts to approve ${subject}.`
  }
  /* istanbul ignore next: name fallback is defensive; callers pass nameKeyFor(sample) */
  const widenTarget = name || 'this package'
  return `${key} is a versioned deny; run \`npm deny-scripts ${widenTarget}\` ` +
    `to widen the deny to all versions of ${widenTarget}, or remove the entry ` +
    `to approve ${subject}.`
}

const isNameOnlyKey = (key) => {
  try {
    const parsed = npa(key)
    if (parsed.type === 'tag') {
      return true
    }
    if (parsed.type === 'range') {
      return parsed.fetchSpec === '*'
        || parsed.rawSpec === ''
        || parsed.rawSpec === '*'
    }
    return false
  } catch {
    /* istanbul ignore next: keys reaching this helper have already parsed via keyTargetsNode */
    return false
  }
}

// Does this policy key target this node by identity (ignoring the
// allow/deny value)?
//
// Registry keys (`tag`, `range`, `version`) require a trusted identity on
// the node. If the node has no `getTrustedRegistryIdentity` result, the
// key does not match — never fall back to `node.name`, which is the
// install-directory name and is forgeable through aliases / manifest
// confusion.
const keyTargetsNode = (key, node) => {
  let parsed
  try {
    parsed = npa(key)
  } catch {
    return false
  }
  switch (parsed.type) {
    case 'tag':
    case 'range':
    case 'version': {
      const trusted = getTrustedRegistryIdentity(node)
      if (!trusted) {
        return false
      }
      return trusted.name === parsed.name
    }
    case 'git': {
      let resolvedParsed
      try {
        resolvedParsed = node.resolved ? npa(node.resolved) : null
      } catch {
        /* istanbul ignore next */
        return false
      }
      const keyHost = parsed.hosted?.ssh({ noCommittish: true })
      const nodeHost = resolvedParsed?.hosted?.ssh({ noCommittish: true })
      return !!(keyHost && nodeHost && keyHost === nodeHost)
    }
    case 'file':
    case 'directory':
    case 'remote':
      return node.resolved === parsed.saveSpec || node.resolved === parsed.fetchSpec
    default:
      return false
  }
}

// Apply approvals for all currently-installed versions of a single package.
//
// `nodes` must all share an identity (same package name for registry deps,
// or same hosted shortcut for git deps, etc.). The caller is responsible
// for grouping nodes correctly.
//
// Returns `{ allowScripts, changes, warning }` where:
//   - `allowScripts` is the new object (the input is never mutated)
//   - `changes` is a list of `{ key, change }` entries describing edits
//   - `warning` is an optional message to surface to the user
const applyApprovalForPackage = (existing, nodes, { pin = true } = {}) => {
  const allowScripts = { ...existing }
  const changes = []

  if (!Array.isArray(nodes) || nodes.length === 0) {
    return { allowScripts, changes }
  }

  const sample = nodes[0]
  const name = nameKeyFor(sample)

  // Deny-wins: any existing false that targets any installed version aborts.
  for (const node of nodes) {
    for (const [key, value] of Object.entries(allowScripts)) {
      if (value === false && keyTargetsNode(key, node)) {
        /* istanbul ignore next: name fallback covers the empty-name edge case */
        const subject = name || 'this package'
        return {
          allowScripts,
          changes,
          warning: denyWarning(key, subject, name),
        }
      }
    }
  }

  if (!pin) {
    // Name-only mode: collapse any single-version pins for this package
    // into a single name-only entry.
    for (const key of Object.keys(allowScripts)) {
      if (
        keyTargetsNode(key, sample) &&
        key !== name &&
        isSingleVersionPin(key) &&
        allowScripts[key] === true
      ) {
        delete allowScripts[key]
      }
    }

    /* istanbul ignore else: name === null is the no-identity path tested separately */
    if (name && allowScripts[name] !== true) {
      allowScripts[name] = true
      changes.push({ key: name, change: 'added' })
    }
    return { allowScripts, changes }
  }

  // Pin mode. For each currently installed version, write a single-version
  // pin if one is not already in place. Stale single-version pins for this
  // package are removed. Per the RFC's pin-mismatch table, an existing
  // name-only entry (`pkg: true`) is replaced by `pkg@x.y.z: true` once
  // every installed version has a pin.
  const installedKeys = new Set(nodes.map(versionedKeyFor).filter(Boolean))

  for (const key of Object.keys(allowScripts)) {
    if (
      keyTargetsNode(key, sample) &&
      isSingleVersionPin(key) &&
      allowScripts[key] === true &&
      !installedKeys.has(key)
    ) {
      delete allowScripts[key]
      changes.push({ key, change: 'removed-stale' })
    }
  }

  for (const key of installedKeys) {
    if (allowScripts[key] !== true) {
      allowScripts[key] = true
      changes.push({ key, change: 'added' })
    }
  }

  // Upgrade: drop the name-only entry once every installed version has a
  // pin. The operation is convergent: running the command twice produces
  // the same shape regardless of the starting state.
  if (
    installedKeys.size > 0 &&
    name &&
    !installedKeys.has(name) &&
    allowScripts[name] === true
  ) {
    delete allowScripts[name]
    changes.push({ key: name, change: 'replaced-by-pin' })
  }

  return { allowScripts, changes }
}

// Apply a deny for a single package. Always name-only; ignores `--allow-scripts-pin`.
const applyDenyForPackage = (existing, nodes) => {
  const allowScripts = { ...existing }
  const changes = []

  if (!Array.isArray(nodes) || nodes.length === 0) {
    return { allowScripts, changes }
  }

  const sample = nodes[0]
  const name = nameKeyFor(sample)
  if (!name) {
    return { allowScripts, changes }
  }

  // Drop any pinned allow entries for this package: the name-only deny
  // overrides them anyway, and leaving them in place is confusing.
  for (const key of Object.keys(allowScripts)) {
    if (keyTargetsNode(key, sample) && key !== name) {
      delete allowScripts[key]
      changes.push({ key, change: 'removed-pinned-allow' })
    }
  }

  if (allowScripts[name] !== false) {
    allowScripts[name] = false
    changes.push({ key: name, change: 'added' })
  }
  return { allowScripts, changes }
}

module.exports = {
  applyApprovalForPackage,
  applyDenyForPackage,
  versionedKeyFor,
  nameKeyFor,
  keyTargetsNode,
  isSingleVersionPin,
}
