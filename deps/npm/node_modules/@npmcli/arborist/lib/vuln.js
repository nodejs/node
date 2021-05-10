// An object representing a vulnerability either as the result of an
// advisory or due to the package in question depending exclusively on
// vulnerable versions of a dep.
//
// - name: package name
// - range: Set of vulnerable versions
// - nodes: Set of nodes affected
// - effects: Set of vulns triggered by this one
// - advisories: Set of advisories (including metavulns) causing this vuln.
//   All of the entries in via are vulnerability objects returned by
//   @npmcli/metavuln-calculator
// - via: dependency vulns which cause this one

const {satisfies, simplifyRange} = require('semver')
const semverOpt = { loose: true, includePrerelease: true }

const npa = require('npm-package-arg')
const _range = Symbol('_range')
const _simpleRange = Symbol('_simpleRange')
const _fixAvailable = Symbol('_fixAvailable')

const severities = new Map([
  ['info', 0],
  ['low', 1],
  ['moderate', 2],
  ['high', 3],
  ['critical', 4],
  [null, -1],
])

for (const [name, val] of severities.entries())
  severities.set(val, name)

class Vuln {
  constructor ({ name, advisory }) {
    this.name = name
    this.via = new Set()
    this.advisories = new Set()
    this.severity = null
    this.effects = new Set()
    this.topNodes = new Set()
    this[_range] = null
    this[_simpleRange] = null
    this.nodes = new Set()
    // assume a fix is available unless it hits a top node
    // that locks it in place, setting this to false or {isSemVerMajor, version}.
    this[_fixAvailable] = true
    this.addAdvisory(advisory)
    this.packument = advisory.packument
    this.versions = advisory.versions
  }

  get fixAvailable () {
    return this[_fixAvailable]
  }

  set fixAvailable (f) {
    this[_fixAvailable] = f
    // if there's a fix available for this at the top level, it means that
    // it will also fix the vulns that led to it being there.  to get there,
    // we set the vias to the most "strict" of fix availables.
    // - false: no fix is available
    // - {name, version, isSemVerMajor} fix requires -f, is semver major
    // - {name, version} fix requires -f, not semver major
    // - true: fix does not require -f
    for (const v of this.via) {
      // don't blow up on loops
      if (v.fixAvailable === f)
        continue

      if (f === false)
        v.fixAvailable = f
      else if (v.fixAvailable === true)
        v.fixAvailable = f
      else if (typeof f === 'object' && (
        typeof v.fixAvailable !== 'object' || !v.fixAvailable.isSemVerMajor))
        v.fixAvailable = f
    }
  }

  testSpec (spec) {
    const specObj = npa(spec)
    if (!specObj.registry)
      return true

    if (specObj.subSpec)
      spec = specObj.subSpec.rawSpec

    for (const v of this.versions) {
      if (satisfies(v, spec) && !satisfies(v, this.range, semverOpt))
        return false
    }
    return true
  }

  toJSON () {
    // sort so that they're always in a consistent order
    return {
      name: this.name,
      severity: this.severity,
      // just loop over the advisories, since via is only Vuln objects,
      // and calculated advisories have all the info we need
      via: [...this.advisories].map(v => v.type === 'metavuln' ? v.dependency : {
        ...v,
        versions: undefined,
        vulnerableVersions: undefined,
        id: undefined,
      }).sort((a, b) =>
        String(a.source || a).localeCompare(String(b.source || b, 'en'))),
      effects: [...this.effects].map(v => v.name)
        .sort(/* istanbul ignore next */(a, b) => a.localeCompare(b, 'en')),
      range: this.simpleRange,
      nodes: [...this.nodes].map(n => n.location)
        .sort(/* istanbul ignore next */(a, b) => a.localeCompare(b, 'en')),
      fixAvailable: this[_fixAvailable],
    }
  }

  addVia (v) {
    this.via.add(v)
    v.effects.add(this)
    // call the setter since we might add vias _after_ setting fixAvailable
    this.fixAvailable = this.fixAvailable
  }

  deleteVia (v) {
    this.via.delete(v)
    v.effects.delete(this)
  }

  deleteAdvisory (advisory) {
    this.advisories.delete(advisory)
    // make sure we have the max severity of all the vulns causing this one
    this.severity = null
    this[_range] = null
    this[_simpleRange] = null
    // refresh severity
    for (const advisory of this.advisories)
      this.addAdvisory(advisory)

    // remove any effects that are no longer relevant
    const vias = new Set([...this.advisories].map(a => a.dependency))
    for (const via of this.via) {
      if (!vias.has(via.name))
        this.deleteVia(via)
    }
  }

  addAdvisory (advisory) {
    this.advisories.add(advisory)
    const sev = severities.get(advisory.severity)
    this[_range] = null
    this[_simpleRange] = null
    if (sev > severities.get(this.severity))
      this.severity = advisory.severity
  }

  get range () {
    return this[_range] ||
      (this[_range] = [...this.advisories].map(v => v.range).join(' || '))
  }

  get simpleRange () {
    if (this[_simpleRange] && this[_simpleRange] === this[_range])
      return this[_simpleRange]

    const versions = [...this.advisories][0].versions
    const range = this.range
    const simple = simplifyRange(versions, range, semverOpt)
    return this[_simpleRange] = this[_range] = simple
  }

  isVulnerable (node) {
    if (this.nodes.has(node))
      return true

    const { version } = node.package
    if (!version)
      return false

    for (const v of this.advisories) {
      if (v.testVersion(version)) {
        this.nodes.add(node)
        return true
      }
    }

    return false
  }
}

module.exports = Vuln
