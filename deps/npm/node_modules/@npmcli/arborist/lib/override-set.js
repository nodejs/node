const npa = require('npm-package-arg')
const semver = require('semver')
const { log } = require('proc-log')

class OverrideSet {
  constructor ({ overrides, key, parent }) {
    this.parent = parent
    this.children = new Map()

    if (typeof overrides === 'string') {
      overrides = { '.': overrides }
    }

    // change a literal empty string to * so we can use truthiness checks on
    // the value property later
    if (overrides['.'] === '') {
      overrides['.'] = '*'
    }

    if (parent) {
      const spec = npa(key)
      if (!spec.name) {
        throw new Error(`Override without name: ${key}`)
      }

      this.name = spec.name
      spec.name = ''
      this.key = key
      this.keySpec = spec.toString()
      this.value = overrides['.'] || this.keySpec
    }

    for (const [key, childOverrides] of Object.entries(overrides)) {
      if (key === '.') {
        continue
      }

      const child = new OverrideSet({
        parent: this,
        key,
        overrides: childOverrides,
      })

      this.children.set(child.key, child)
    }
  }

  childrenAreEqual (other) {
    if (this.children.size !== other.children.size) {
      return false
    }
    for (const [key] of this.children) {
      if (!other.children.has(key)) {
        return false
      }
      if (this.children.get(key).value !== other.children.get(key).value) {
        return false
      }
      if (!this.children.get(key).childrenAreEqual(other.children.get(key))) {
        return false
      }
    }
    return true
  }

  isEqual (other) {
    if (this === other) {
      return true
    }
    if (!other) {
      return false
    }
    if (this.key !== other.key || this.value !== other.value) {
      return false
    }
    if (!this.childrenAreEqual(other)) {
      return false
    }
    if (!this.parent) {
      return !other.parent
    }
    return this.parent.isEqual(other.parent)
  }

  getEdgeRule (edge) {
    for (const rule of this.ruleset.values()) {
      if (rule.name !== edge.name) {
        continue
      }

      // if keySpec is * we found our override
      if (rule.keySpec === '*') {
        return rule
      }

      // We need to use the rawSpec here, because the spec has the overrides applied to it already.
      // rawSpec can be undefined, so we need to use the fallback value of spec if it is.
      let spec = npa(`${edge.name}@${edge.rawSpec || edge.spec}`)
      if (spec.type === 'alias') {
        spec = spec.subSpec
      }

      if (spec.type === 'git') {
        if (spec.gitRange && semver.intersects(spec.gitRange, rule.keySpec)) {
          return rule
        }

        continue
      }

      if (spec.type === 'range' || spec.type === 'version') {
        if (semver.intersects(spec.fetchSpec, rule.keySpec)) {
          return rule
        }

        continue
      }

      // if we got this far, the spec type is one of tag, directory or file
      // which means we have no real way to make version comparisons, so we
      // just accept the override
      return rule
    }

    return this
  }

  getNodeRule (node) {
    for (const rule of this.ruleset.values()) {
      if (rule.name !== node.name) {
        continue
      }

      if (semver.satisfies(node.version, rule.keySpec) ||
        semver.satisfies(node.version, rule.value)) {
        return rule
      }
    }

    return this
  }

  getMatchingRule (node) {
    for (const rule of this.ruleset.values()) {
      if (rule.name !== node.name) {
        continue
      }

      if (semver.satisfies(node.version, rule.keySpec) ||
        semver.satisfies(node.version, rule.value)) {
        return rule
      }
    }

    return null
  }

  * ancestry () {
    for (let ancestor = this; ancestor; ancestor = ancestor.parent) {
      yield ancestor
    }
  }

  get isRoot () {
    return !this.parent
  }

  get ruleset () {
    const ruleset = new Map()

    for (const override of this.ancestry()) {
      for (const kid of override.children.values()) {
        if (!ruleset.has(kid.key)) {
          ruleset.set(kid.key, kid)
        }
      }

      if (!override.isRoot && !ruleset.has(override.key)) {
        ruleset.set(override.key, override)
      }
    }

    return ruleset
  }

  static findSpecificOverrideSet (first, second) {
    for (let overrideSet = second; overrideSet; overrideSet = overrideSet.parent) {
      if (overrideSet.isEqual(first)) {
        return second
      }
    }
    for (let overrideSet = first; overrideSet; overrideSet = overrideSet.parent) {
      if (overrideSet.isEqual(second)) {
        return first
      }
    }

    // The override sets are incomparable. Neither one contains the other.
    log.silly('Conflicting override sets', first, second)
  }

  static doOverrideSetsConflict (first, second) {
    // If override sets contain one another then we can try to use the more specific one.
    // If neither one is more specific, check for semantic conflicts.
    const specificSet = this.findSpecificOverrideSet(first, second)
    if (specificSet !== undefined) {
      // One contains the other, so no conflict
      return false
    }

    // The override sets are structurally incomparable, but this doesn't necessarily
    // mean they conflict. We need to check if they have conflicting version requirements
    // for any package that appears in both rulesets.
    return this.haveConflictingRules(first, second)
  }

  static haveConflictingRules (first, second) {
    // Get all rules from both override sets
    const firstRules = first.ruleset
    const secondRules = second.ruleset

    // Check each package that appears in both rulesets
    for (const [key, firstRule] of firstRules) {
      const secondRule = secondRules.get(key)
      if (!secondRule) {
        // Package only appears in one ruleset, no conflict
        continue
      }

      // Same rule object means no conflict
      if (firstRule === secondRule || firstRule.isEqual(secondRule)) {
        continue
      }

      // Both rulesets have rules for this package with different values.
      // Check if the version requirements are actually incompatible.
      const firstValue = firstRule.value
      const secondValue = secondRule.value

      // If either value is a reference (starts with $), we can't determine
      // compatibility here - the reference might resolve to compatible versions.
      // We defer to runtime resolution rather than failing early.
      if (firstValue.startsWith('$') || secondValue.startsWith('$')) {
        continue
      }

      // Check if the version ranges are compatible using semver
      // If both specify version ranges, they conflict only if they have no overlap
      try {
        const firstSpec = npa(`${firstRule.name}@${firstValue}`)
        const secondSpec = npa(`${secondRule.name}@${secondValue}`)

        // For range/version types, check if they intersect
        if ((firstSpec.type === 'range' || firstSpec.type === 'version') &&
            (secondSpec.type === 'range' || secondSpec.type === 'version')) {
          // Check if the ranges intersect
          const firstRange = firstSpec.fetchSpec
          const secondRange = secondSpec.fetchSpec

          // If the ranges don't intersect, we have a real conflict
          if (!semver.intersects(firstRange, secondRange)) {
            log.silly('Found conflicting override rules', {
              package: firstRule.name,
              first: firstValue,
              second: secondValue,
            })
            return true
          }
        }
        // For other types (git, file, directory, tag), we can't easily determine
        // compatibility, so we conservatively assume no conflict
      } catch {
        // If we can't parse the specs, conservatively assume no conflict
        // Real conflicts will be caught during dependency resolution
      }
    }

    // No conflicting rules found
    return false
  }
}

module.exports = OverrideSet
