// Parse an `allow-scripts` raw config value (string or array of strings)
// into a flat array of trimmed package-spec entries. Shared between the
// CLI/env layer (via the `allow-scripts` definition's `flatten`) and the
// package.json / .npmrc layer (in lib/utils/resolve-allow-scripts.js) so
// both paths agree on quoting, whitespace, and duplicate handling.
const parseAllowScriptsList = (raw) => {
  const parts = []
  const entries = Array.isArray(raw) ? raw : (typeof raw === 'string' ? [raw] : [])
  for (const entry of entries) {
    if (typeof entry !== 'string') {
      continue
    }
    for (const part of entry.split(',')) {
      const trimmed = part.trim()
      if (trimmed) {
        parts.push(trimmed)
      }
    }
  }
  return parts
}

module.exports = parseAllowScriptsList
