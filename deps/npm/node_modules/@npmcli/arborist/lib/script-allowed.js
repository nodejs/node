const npa = require('npm-package-arg')
const semver = require('semver')
const versionFromTgz = require('./version-from-tgz.js')

// Identity matcher for the allowScripts policy.
//
// Returns:
//   - true: at least one allow entry matches and no deny entry matches
//   - false: at least one deny entry matches (deny wins on conflict)
//   - null: no entry matches (unreviewed)
//
// `policy` is a flat object of `spec-key -> boolean`, where spec-key is
// anything `npm-package-arg` can parse. `node` is an arborist Node.
//
// Identity rules (see RFC npm/rfcs#868):
//   - registry deps match by the name+version parsed from the lockfile's
//     resolved URL, NOT by `node.packageName` / `node.version`. Those two
//     getters return `node.package.name` / `node.package.version`, which
//     come from the tarball's own package.json and are therefore
//     attacker-controlled. A package can publish a tarball claiming any
//     name; the only trusted name is the one baked into the registry URL.
//   - tarball / file / link / remote: exact match on node.resolved
//   - git: match on hosted.ssh() plus a short-SHA prefix of the
//     resolved committish

const isScriptAllowed = (node, policy) => {
  // Bundled dependencies never run their install scripts and cannot be
  // allowlisted. Matching by name@version from the bundled tarball would
  // reintroduce manifest confusion (a bundled tarball can claim any name
  // and version). Returning null marks them as not-allowed regardless of
  // any policy entry, so their install scripts are blocked by the
  // install-time gate. A package that needs a bundled dep's script must
  // forward it as one of its own lifecycle scripts.
  if (node.inBundle) {
    return null
  }

  if (!policy || typeof policy !== 'object') {
    return null
  }

  let anyAllow = false
  let anyDeny = false

  for (const [key, value] of Object.entries(policy)) {
    // Pass deny intent so matchRegistry can fail closed on an unverifiable
    // version: a deny still blocks, an allow stays refused.
    if (!matches(node, key, value === false)) {
      continue
    }
    if (value === false) {
      anyDeny = true
      continue
    }
    /* istanbul ignore else: policy values are strictly true/false;
       defensive guard against unexpected coercions. */
    if (value === true) {
      anyAllow = true
    }
  }

  if (anyDeny) {
    return false
  }
  if (anyAllow) {
    return true
  }
  return null
}

const matches = (node, key, failClosed) => {
  let parsed
  try {
    parsed = npa(key)
  } catch {
    return false
  }

  switch (parsed.type) {
    case 'tag':
    case 'range':
    case 'version':
      return matchRegistry(node, parsed, failClosed)
    case 'git':
      return matchGit(node, parsed)
    case 'file':
    case 'directory':
      return matchFileOrDir(node, parsed)
    case 'remote':
      return matchRemote(node, parsed)
    case 'alias':
      // Disallowed: aliases as policy keys do not match anything.
      // The user has to address the real package name.
      return false
    /* istanbul ignore next: switch above covers every npa type we expect;
       defensive fallback for future npa types. */
    default:
      return false
  }
}

const resolvedSourceSpecs = (node) => {
  const specs = []
  const seen = new Set()
  const add = (spec) => {
    if (typeof spec !== 'string' || spec === '' || seen.has(spec)) {
      return
    }
    seen.add(spec)
    specs.push(spec)
  }

  add(node?.resolved)

  if (!node?.resolved && node?.linksIn && typeof node.linksIn[Symbol.iterator] === 'function') {
    let hasIncomingLink = false
    for (const link of node.linksIn) {
      hasIncomingLink = true
      add(link.resolved)
    }

    if (hasIncomingLink) {
      // Link targets for local directory deps are separate inventory nodes
      // whose own `resolved` is null. The incoming Link carries the saved spec
      // (for example `file:../pkg`, relative to node_modules), while policy
      // entries written by hand often use the dependency spec from package.json
      // (for example `file:pkg`, resolved by npa to this target path). Include
      // the real target paths so both forms can match the same local dep.
      add(node.realpath)
      add(node.path)
    }
  }

  return specs
}

const matchRegistry = (node, parsed, failClosed) => {
  // If this node is not a registry dep, refuse the match. A registry-style
  // key (`pkg`, `pkg@1`, `pkg@1 || 2`) must not match a tarball or git node
  // even if their names happen to coincide.
  if (!isRegistryNode(node)) {
    return false
  }

  // Derive the trusted name+version from the lockfile's resolved URL.
  // Never use `node.packageName` / `node.version` here: those read from
  // the tarball's own package.json and can be forged by a malicious
  // publisher to bypass an allowScripts entry.
  const trusted = getTrustedRegistryIdentity(node)
  if (!trusted || trusted.name !== parsed.name) {
    return false
  }

  // `tag` covers `pkg@latest`. Rejected up front by validatePolicy in
  // resolve-allow-scripts.js because tags look like a pin but can't be
  // verified at install time. Defense-in-depth: if one slips through
  // (e.g. arborist invoked directly without the resolver), don't match.
  if (parsed.type === 'tag') {
    /* istanbul ignore next: validatePolicy filters this; defensive */
    return false
  }

  // `range` includes `pkg@^1`, `pkg@1 || 2`, `pkg@*`, `pkg@>=0`, and bare
  // names like `pkg` (npa parses these as range with fetchSpec='*'). The
  // RFC permits bare names (name-only allow) and exact versions joined by
  // `||`; ranges like ^/~/>=/< are rejected because they would silently
  // allow versions the user has never reviewed.
  if (parsed.type === 'range') {
    // Bare name or `pkg@*`: treat as name-only allow.
    if (parsed.fetchSpec === '*' || parsed.rawSpec === '' || parsed.rawSpec === '*') {
      return true
    }
    if (!isExactVersionDisjunction(parsed.fetchSpec)) {
      return false
    }
    // Unverifiable version (omit-lockfile-registry-resolved): a deny blocks,
    // an allow is refused.
    if (!trusted.version) {
      return failClosed
    }
    return semver.satisfies(trusted.version, parsed.fetchSpec, { loose: true })
  }

  // `version` is an exact pin like `pkg@1.2.3`.
  /* istanbul ignore else: parsed.type at this point is always 'version';
     the istanbul-ignored fallback below handles the impossible case. */
  if (parsed.type === 'version') {
    // Unverifiable version: a deny blocks, an allow is refused.
    if (!trusted.version) {
      return failClosed
    }
    return trusted.version === parsed.fetchSpec
  }

  /* istanbul ignore next: parsed.type is constrained to tag/range/version
     by the caller; this final fallback is defensive. */
  return false
}

// Derive a registry node's trusted name+version.
//
// Preferred source: the lockfile's resolved URL parsed via
// versionFromTgz. arborist records the URL when it first adds the dep,
// before any tarball is unpacked, so the URL cannot be forged by the
// package's own package.json.
//
// Fallback for lockfiles produced with omit-lockfile-registry-resolved
// (where the URL is absent): take the dep name from an incoming
// dependency edge. The edge's spec was written by the consumer (or by an
// upstream package.json), not by the installed tarball. For aliases like
// `"trusted": "npm:naughty@1.0.0"`, the underlying registered package
// name is parsed out of the alias `subSpec`. The install location
// (`node_modules/trusted`) is deliberately not consulted because for
// aliases it carries only the alias name, which would let a malicious
// publisher bypass an allowScripts entry written for the real package.
//
// Version is left null in the fallback case because the only remaining
// source for it (`node.version`) reads from the tarball.
//
// Returns `{ name, version }` or `null` if no trusted identity exists.
const getTrustedRegistryIdentity = (node) => {
  if (node.resolved && typeof node.resolved === 'string') {
    const parsed = versionFromTgz('', node.resolved)
    /* istanbul ignore else: versionFromTgz returns either a complete
       { name, version } or null; partial objects are not produced. */
    if (parsed && parsed.name && parsed.version) {
      return parsed
    }
  }
  const name = nameFromEdges(node)
  if (name) {
    return { name, version: null }
  }
  return null
}

const nameFromEdges = (node) => {
  if (!node.edgesIn || typeof node.edgesIn[Symbol.iterator] !== 'function') {
    return null
  }
  for (const edge of node.edgesIn) {
    let parsed
    try {
      parsed = npa.resolve(edge.name, edge.spec)
    } catch {
      continue
    }
    // Aliases: trust the underlying registered package, not the alias.
    if (parsed.type === 'alias' && parsed.subSpec && parsed.subSpec.registry) {
      return parsed.subSpec.name
    }
    // Non-aliased registry edge: the edge name is the package name as
    // written by the consumer / upstream, which is trusted (it is not
    // read from the installed tarball).
    if (parsed.registry) {
      return parsed.name
    }
  }
  return null
}

// True if `rangeSpec` is one or more exact versions joined by `||`. Anything
// containing comparator operators (^, ~, >=, <, *) returns false.
const isExactVersionDisjunction = (rangeSpec) => {
  /* istanbul ignore next: caller always passes parsed.fetchSpec, which
     npa guarantees to be a non-empty string for range specs. */
  if (typeof rangeSpec !== 'string' || rangeSpec.trim() === '') {
    return false
  }
  const parts = rangeSpec.split('||').map(p => p.trim())
  /* istanbul ignore next: String.prototype.split always returns at least
     one element; defensive guard only. */
  if (parts.length === 0) {
    return false
  }
  return parts.every(p => p !== '' && semver.valid(p) !== null)
}

const matchGit = (node, parsed) => {
  if (!node.resolved || !node.resolved.startsWith('git')) {
    return false
  }

  let nodeParsed
  try {
    nodeParsed = npa(node.resolved)
  } catch {
    /* istanbul ignore next: npa parsing a git URL we already validated
       starts with `git` should not throw; defensive guard only. */
    return false
  }

  // Compare the host/repo. Both sides should resolve to the same canonical
  // ssh URL.
  const noCommittish = { noCommittish: true }
  const keyHost = parsed.hosted?.ssh(noCommittish)
  const nodeHost = nodeParsed.hosted?.ssh(noCommittish)
  if (keyHost && nodeHost) {
    if (keyHost !== nodeHost) {
      return false
    }
  } else if (parsed.fetchSpec && nodeParsed.fetchSpec) {
    // Non-hosted git URLs: fall back to fetch spec.
    if (parsed.fetchSpec !== nodeParsed.fetchSpec) {
      return false
    }
  } else {
    return false
  }

  // If the policy key has no committish, name-only match.
  const keyCommittish = parsed.gitCommittish || parsed.hosted?.committish
  if (!keyCommittish) {
    return true
  }

  // Match the resolved full SHA against the key's committish. Users
  // typically write short SHAs in the policy; the lockfile stores 40-char
  // SHAs. Direction matters: the lockfile's full SHA must START WITH the
  // key's short SHA, never the reverse. A longer key matching a shorter
  // resolved committish would let a malformed lockfile or a divergent
  // resolver allow scripts the user never approved.
  const nodeCommittish = nodeParsed.gitCommittish || nodeParsed.hosted?.committish || ''
  if (!nodeCommittish) {
    return false
  }
  return nodeCommittish.startsWith(keyCommittish)
}

const matchFileOrDir = (node, parsed) => {
  return resolvedSourceSpecs(node)
    .some(resolved => resolved === parsed.saveSpec || resolved === parsed.fetchSpec)
}

const matchRemote = (node, parsed) => {
  return resolvedSourceSpecs(node)
    .some(resolved => resolved === parsed.fetchSpec || resolved === parsed.saveSpec)
}

const isRegistryNode = (node) => {
  // Prefer arborist's edge-based check when available (real Node objects).
  // It inspects the incoming edges' specs and only returns true if every
  // edge resolves to a registry spec, which is much harder to spoof than
  // the URL.
  if (typeof node.isRegistryDependency === 'boolean') {
    return node.isRegistryDependency
  }
  // Fall back to URL parsing for nodes without the arborist getter
  // (e.g. test fixtures, lockfiles with omit-lockfile-registry-resolved).
  // Treat the node as a registry dep when:
  //   - resolved is missing entirely (omitLockfileRegistryResolved),
  //   - resolved is an https/http URL pointing at a registry tarball, or
  //   - resolved is undefined and the node has a version (defensive).
  if (!node.resolved) {
    return !!node.version
  }
  // Registry tarballs live at `<host>/<pkg-name>/-/<pkg-name>-<version>.tgz`.
  // Require a path segment before `/-/` so an attacker can't lift a
  // registry-style allow entry to a hostile URL like
  // `https://evil.com/-/trusted-1.0.0.tgz`.
  return /^https?:\/\/[^/]+\/.+\/-\/[^/]+-\d/.test(node.resolved)
}

// Trusted display identity for human-facing output (the `npm install`
// blocked-scripts summary and `npm approve-scripts --allow-scripts-pending`).
// Same as getTrustedRegistryIdentity, but for display only: version
// falls back to node.version when the URL doesn't carry one. Do not
// use for policy matching.
const trustedDisplay = (node) => {
  const trusted = getTrustedRegistryIdentity(node)
  /* istanbul ignore next: defensive fallbacks for nodes without name/version */
  return {
    name: (trusted && trusted.name) || node.name || null,
    version: (trusted && trusted.version) || node.version || null,
  }
}

module.exports = isScriptAllowed
module.exports.isScriptAllowed = isScriptAllowed
module.exports.matches = matches
module.exports.isExactVersionDisjunction = isExactVersionDisjunction
module.exports.getTrustedRegistryIdentity = getTrustedRegistryIdentity
module.exports.resolvedSourceSpecs = resolvedSourceSpecs
module.exports.trustedDisplay = trustedDisplay
