const hash = require('./hash.js')
const semver = require('semver')
const semverOpt = { includePrerelease: true, loose: true }
const getDepSpec = require('./get-dep-spec.js')

// any fields that we don't want in the cache need to be hidden
const _source = Symbol('source')
const _packument = Symbol('packument')
const _versionVulnMemo = Symbol('versionVulnMemo')
const _updated = Symbol('updated')
const _options = Symbol('options')
const _specVulnMemo = Symbol('specVulnMemo')
const _testVersion = Symbol('testVersion')
const _testVersions = Symbol('testVersions')
const _calculateRange = Symbol('calculateRange')
const _markVulnerable = Symbol('markVulnerable')
const _testSpec = Symbol('testSpec')

class Advisory {
  constructor (name, source, options = {}) {
    this.source = source.id
    this[_source] = source
    this[_options] = options
    this.name = name
    if (!source.name)
      source.name = name

    this.dependency = source.name

    if (this.type === 'advisory') {
      this.title = source.title
      this.url = source.url
    } else {
      this.title = `Depends on vulnerable versions of ${source.name}`
      this.url = null
    }

    this.severity = source.severity
    this.versions = []
    this.vulnerableVersions = []
    // advisories have the range, metavulns do not
    this.range = source.vulnerable_versions || null
    this.id = hash(this)

    this[_packument] = null
    // memoized list of which versions are vulnerable
    this[_versionVulnMemo] = new Map()
    // memoized list of which dependency specs are vulnerable
    this[_specVulnMemo] = new Map()
    this[_updated] = false
  }

  // true if we updated from what we had in cache
  get updated () {
    return this[_updated]
  }

  get type () {
    return this.dependency === this.name ? 'advisory' : 'metavuln'
  }

  get packument () {
    return this[_packument]
  }

  // load up the data from a cache entry and a fetched packument
  load (cached, packument) {
    // basic data integrity gutchecks
    if (!cached || typeof cached !== 'object') {
      throw new TypeError('invalid cached data, expected object')
    }
    if (!packument || typeof packument !== 'object') {
      throw new TypeError('invalid packument data, expected object')
    }
    if (cached.id && cached.id !== this.id) {
      throw Object.assign(new Error('loading from incorrect cache entry'), {
        expected: this.id,
        actual: cached.id,
      })
    }
    if (packument.name !== this.name) {
      throw Object.assign(new Error('loading from incorrect packument'), {
        expected: this.name,
        actual: packument.name,
      })
    }
    if (this[_packument])
      throw new Error('advisory object already loaded')

    // if we have a range from the initialization, and the cached
    // data has a *different* range, then we know we have to recalc.
    // just don't use the cached data, so we will definitely not match later
    if (!this.range || cached.range && cached.range === this.range)
      Object.assign(this, cached)

    this[_packument] = packument

    const pakuVersions = Object.keys(packument.versions)
    const allVersions = new Set([...pakuVersions, ...this.versions])
    const versionsAdded = []
    const versionsRemoved = []
    for (const v of allVersions) {
      if (!this.versions.includes(v)) {
        versionsAdded.push(v)
        this.versions.push(v)
      } else if (!pakuVersions.includes(v)) {
        versionsRemoved.push(v)
      }
    }

    // strip out any removed versions from our lists, and sort by semver
    this.versions = semver.sort(this.versions.filter(v =>
      !versionsRemoved.includes(v)), semverOpt)

    // if no changes, then just return what we got from cache
    // versions added or removed always means we changed
    // otherwise, advisories change if the range changes, and
    // metavulns change if the source was updated
    const unchanged = this.type === 'advisory'
      ? this.range && this.range === cached.range
      : !this[_source].updated

    // if the underlying source changed, by an advisory updating the
    // range, or a source advisory being updated, then we have to re-check
    // otherwise, only recheck the new ones.
    this.vulnerableVersions = !unchanged ? []
      : semver.sort(this.vulnerableVersions.filter(v =>
        !versionsRemoved.includes(v)), semverOpt)

    if (unchanged && !versionsAdded.length && !versionsRemoved.length) {
      // nothing added or removed, nothing to do here.  use the cached copy.
      return this
    }

    this[_updated] = true

    // test any versions newly added
    if (!unchanged || versionsAdded.length)
      this[_testVersions](unchanged ? versionsAdded : this.versions)
    this.vulnerableVersions = semver.sort(this.vulnerableVersions, semverOpt)

    // metavulns have to calculate their range, since cache is invalidated
    // advisories just get their range from the advisory above
    if (this.type === 'metavuln')
      this[_calculateRange]()

    return this
  }

  [_calculateRange] () {
    const metavuln = this.vulnerableVersions.join(' || ').trim()
    this.range = !metavuln ? '<0.0.0-0'
      : semver.simplifyRange(this.versions, metavuln, semverOpt)
  }

  // returns true if marked as vulnerable, false if ok
  // spec is a dependency specifier, for metavuln cases
  // where the version might not be in the packument.  if
  // we have the packument and spec is not provided, then
  // we use the dependency version from the manifest.
  testVersion (version, spec = null) {
    const sv = String(version)
    if (this[_versionVulnMemo].has(sv))
      return this[_versionVulnMemo].get(sv)

    const result = this[_testVersion](version, spec)
    if (result)
      this[_markVulnerable](version)
    this[_versionVulnMemo].set(sv, !!result)
    return result
  }

  [_markVulnerable] (version) {
    const sv = String(version)
    if (!this.vulnerableVersions.includes(sv))
      this.vulnerableVersions.push(sv)
  }

  [_testVersion] (version, spec) {
    const sv = String(version)
    if (this.vulnerableVersions.includes(sv))
      return true

    if (this.type === 'advisory') {
      // advisory, just test range
      return semver.satisfies(version, this.range, semverOpt)
    }

    // check the dependency of this version on the vulnerable dep
    // if we got a version that's not in the packument, fall back on
    // the spec provided, if possible.
    const mani = this[_packument].versions[version] || {
      dependencies: {
        [this.dependency]: spec,
      },
    }

    if (!spec)
      spec = getDepSpec(mani, this.dependency)

    // no dep, no vuln
    if (spec === null)
      return false

    if (!semver.validRange(spec, semverOpt)) {
      // not a semver range, nothing we can hope to do about it
      return true
    }

    const bd = mani.bundleDependencies
    const bundled = bd && bd.includes(this[_source].name)
    // XXX if bundled, then semver.intersects() means vulnerable
    // else, pick a manifest and see if it can't be avoided
    // try to pick a version of the dep that isn't vulnerable
    const avoid = this[_source].range

    if (bundled) {
      return semver.intersects(spec, avoid, semverOpt)
    }

    return this[_source].testSpec(spec)
  }

  testSpec (spec) {
    // testing all the versions is a bit costly, and the spec tends to stay
    // consistent across multiple versions, so memoize this as well, in case
    // we're testing lots of versions.
    const memo = this[_specVulnMemo]
    if (memo.has(spec)) {
      return memo.get(spec)
    }

    const res = this[_testSpec](spec)
    memo.set(spec, res)
    return res
  }

  [_testSpec] (spec) {
    for (const v of this.versions) {
      const satisfies = semver.satisfies(v, spec)
      if (!satisfies)
        continue
      if (!this.testVersion(v))
        return false
    }
    // either vulnerable, or not installable because nothing satisfied
    // either way, best avoided.
    return true
  }

  [_testVersions] (versions) {
    if (!versions.length)
      return

    // set of lists of versions
    const versionSets = new Set()
    versions = semver.sort(versions.map(v => semver.parse(v, semverOpt)))

    // start out with the versions grouped by major and minor
    let last = versions[0].major + '.' + versions[0].minor
    let list = []
    versionSets.add(list)
    for (const v of versions) {
      const k = v.major + '.' + v.minor
      if (k !== last) {
        last = k
        list = []
        versionSets.add(list)
      }
      list.push(v)
    }

    for (const list of versionSets) {
      const headVuln = this.testVersion(list[0])
      const tailVuln = this.testVersion(list[list.length - 1])
      // if head and tail both vulnerable, whole list is thrown out
      if (headVuln && tailVuln) {
        for (const v of list.slice(1, -1)) {
          this[_markVulnerable](v)
        }
        continue
      }

      // if length is 2 or 1, then we marked them all already
      if (list.length <= 2)
        continue

      const mid = Math.floor(list.length / 2)
      // leave out the ends, since we tested those already
      versionSets.add(list.slice(0, mid))
      versionSets.add(list.slice(mid))
    }
  }
}
module.exports = Advisory
