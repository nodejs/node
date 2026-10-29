// Determine whether a package name is exempt from the `min-release-age` /
// `before` release-age filter, based on the `min-release-age-exclude` config.
//
// Patterns are exact package names or `minimatch` globs (e.g. `@myorg/*`), and
// match against the package name only. This is a "named-only" exemption: a
// matched package's own dependencies still follow the filter unless they match
// a pattern too.
//
// Callers must match against the resolved registry identity of a package, not
// the self-reported alias or dependency-edge name. For `npm:` aliases the
// fetched package is the alias target, so run specs through `trustedSpecName`
// first; otherwise an alias key could match an exclude pattern and turn the
// filter off for the unrelated package it resolves to.
const { minimatch } = require('minimatch')

// This list only ever widens the exemption (turns the security filter off for a
// package), so disable pattern features that could silently turn it into a
// match-all: `nonegate` keeps a leading `!` literal (so a stray `!foo` exempts
// nothing instead of everything-but-foo), `nocomment` keeps a leading `#`
// literal, and `noext` disables extglobs.
const minimatchOptions = { nonegate: true, nocomment: true, noext: true }

const isReleaseAgeExcluded = (name, patterns) => {
  if (!name || !Array.isArray(patterns) || patterns.length === 0) {
    return false
  }
  return patterns.some(pattern =>
    name === pattern || minimatch(name, pattern, minimatchOptions))
}

// Resolve the trusted registry name for an npa spec. For `npm:` aliases (e.g.
// `"x": "npm:other@1"`) the installed/fetched package is the alias target
// (`subSpec`), not the alias key, so the exemption must be keyed on the
// underlying package name. Mirrors `nameFromEdges` in script-allowed.js.
const trustedSpecName = (spec) => {
  if (!spec) {
    return undefined
  }
  if (spec.type === 'alias' && spec.subSpec && spec.subSpec.registry) {
    return spec.subSpec.name
  }
  return spec.name
}

module.exports = { isReleaseAgeExcluded, trustedSpecName }
