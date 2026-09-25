const npa = require('npm-package-arg')
const { matches } = require('@npmcli/arborist/lib/script-allowed.js')

// Pure classifier behind `npm install-scripts prune`.
//
// Splits the `package.json#allowScripts` map into entries to keep (`remaining`)
// and unused ones to drop (`removed`). `nodes` is the install candidates as
// `{ node, hasScripts }`; the caller gathers them so this stays sync.
//
// An entry is unused (version-aware, by trusted identity) when:
//   - no installed node matches the key  -> reason 'not-installed'
//   - it matches but none have scripts   -> reason 'no-scripts'
//
// Unparseable keys are kept (prune never drops what it can't parse). Both
// `true` (approve) and `false` (deny) entries are pruned.
const classifyUnusedEntries = (allowScripts, nodes) => {
  const remaining = {}
  const removed = []

  for (const [key, value] of Object.entries(allowScripts || {})) {
    let parseable = true
    try {
      npa(key)
    } catch {
      parseable = false
    }
    if (!parseable) {
      remaining[key] = value
      continue
    }

    const matching = nodes.filter(({ node }) => matches(node, key))
    if (matching.length === 0) {
      removed.push({ key, value, reason: 'not-installed' })
      continue
    }
    if (!matching.some(({ hasScripts }) => hasScripts)) {
      removed.push({ key, value, reason: 'no-scripts' })
      continue
    }
    remaining[key] = value
  }

  return { remaining, removed }
}

module.exports = { classifyUnusedEntries }
