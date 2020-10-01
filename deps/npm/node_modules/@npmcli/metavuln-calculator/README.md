# @npmcli/metavuln-calculator

Calculate meta-vulnerabilities from package security advisories

This is a pretty low-level package to abstract out the parts of
[@npmcli/arborist](http://npm.im/@npmcli/arborist) that calculate
metavulnerabilities from security advisories.  If you just want to get an
audit for a package tree, probably what you want to use is
`arborist.audit()`.

## USAGE

```js
const Calculator = require('@npmcli/metavuln-calculator')
// pass in any options for cacache and pacote
// see those modules for option descriptions
const calculator = new Calculator(options)

// get an advisory somehow, typically by POSTing a JSON payload like:
// {"pkgname":["1.2.3","4.3.5", ...versions], ...packages}
// to /-/npm/v1/security/advisories/bulk
// to get a payload response like:
// {
//   "semver": [
//     {
//       "id": 31,
//       "url": "https://npmjs.com/advisories/31",
//       "title": "Regular Expression Denial of Service",
//       "severity": "moderate",
//       "vulnerable_versions": "<4.3.2"
//     }
//   ],
//   ...advisories
// }
const arb = new Aborist(options)
const tree = await arb.loadActual()
const advisories = await getBulkAdvisoryReportSomehow(tree)

// then to get a comprehensive set of advisories including metavulns:
const set = new Set()
for (const [name, advisory] of Object.entries(advisories)) {
  // make sure we have the advisories loaded with latest version lists
  set.add(await calculator.calculate(name, {advisory}))
}

for (const vuln of set) {
  for (const node of tree.inventory.query('name', vuln.name)) {
    // not vulnerable, just keep looking
    if (!vuln.testVersion(node.version))
      continue
    for (const { from: dep, spec } of node.edgesIn) {
      const metaAdvisory = await calculator.calculate(dep.name, vuln)
      if (metaAdvisory.testVersion(dep.version, spec)) {
        set.add(metaAdvisory)
      }
    }
  }
}
```

## API

### Class: Advisory

The `Calculator.calculate` method returns a Promise that resolves to a
`Advisory` object, filled in from the cache and updated if necessary with
the available advisory data.

Do not instantiate `Advisory` objects directly.  Use the `calculate()`
method to get one with appropriate data filled in.

Do not mutate `Advisory` objects.  Use the supplied methods only.

#### Fields

- `name` The name of the package that this vulnerability is about
- `id` The unique cache key for this vuln or metavuln.  (See **Cache Keys**
  below.)
- `dependency` For metavulns, the dependency that causes this package to be
  have a vulnerability.  For advisories, the same as `name`.
- `type` Either `'advisory'` or `'metavuln'`, depending on the type of
  vulnerability that this object represents.
- `url` The url for the advisory (`null` for metavulns)
- `title` The text title of the advisory or metavuln
- `severity` The severity level info/low/medium/high/critical
- `range` The range that is vulnerable
- `versions` The set of available versions of the package
- `vulnerableVersions` The set of versions that are vulnerable
- `source` The numeric ID of the advisory, or the cache key of the
  vulnerability that causes this metavuln
- `updated` Boolean indicating whether this vulnerability was updated since
  being read from cache.
- `packument` The packument object for the package that this vulnerability
  is about.

#### `vuln.testVersion(version, [dependencySpecifier]) -> Boolean`

Check to see if a given version is vulnerable.  Returns `true` if the
version is vulnerable, and should be avoided.

For metavulns, `dependencySpecifier` indicates the version range of the
source of the vulnerability, which the module depends on.  If not provided,
will attempt to read from the packument.  If not provided, and unable to
read from the packument, then `true` is returned, indicating that the (not
installable) package version should be avoided.

#### Cache Keys

The cache keys are calculated by hashing together the `source` and `name`
fields, prefixing with the string `'security-advisory:'` and the name of
the dependency that is vulnerable.

So, a third-level metavulnerability might have a key like:

```
'security-advisory:foo:'+ hash(['foo', hash(['bar', hash(['baz', 123])])])
```

Thus, the cached entry with this key would reflect the version of `foo`
that is vulnerable by virtue of dependending exclusively on versions of
`bar` which are vulnerable by virtue of depending exclusively on versions
of `baz` which are vulnerable by virtue of advisory ID `123`.

Loading advisory data entirely from cache without hitting an npm registry
security advisory endpoint is not supported at this time, but technically
possible, and likely to come in a future version of this library.

### `calculator = new Calculator(options)`

Options object is used for `cacache` and `pacote` calls.

### `calculator.calculate(name, source)`

- `name` The name of the package that the advisory is about
- `source` Advisory object from the npm security endpoint, or a `Advisory`
  object returned by a previous call to the `calculate()` method.
  "Advisory" objects need to have:
  - `id` id of the advisory or Advisory object
  - `vulnerable_versions` range of versions affected
  - `url`
  - `title`
  - `severity`

Fetches the packument and returns a Promise that resolves to a
vulnerability object described above.

Will perform required I/O to fetch package metadata from registry and
read from cache.  Advisory information written back to cache.

## Dependent Version Sampling

Typically, dependency ranges don't change very frequently, and the most
recent version published on a given release line is most likely to contain
the fix for a given vulnerability.

So, we see things like this:

```
3.0.4 - not vulnerable
3.0.3 - vulnerable
3.0.2 - vulnerable
3.0.1 - vulnerable
3.0.0 - vulnerable
2.3.107 - not vulnerable
2.3.106 - not vulnerable
2.3.105 - vulnerable
... 523 more vulnerable versions ...
2.0.0 - vulnerable
1.1.102 - not vulnerable
1.1.101 - vulnerable
... 387 more vulnerable versions ...
0.0.0 - vulnerable
```

In order to determine which versions of a package are affected by a
vulnerability in a dependency, this module uses the following algorithm to
minimize the number of tests required by performing a binary search on each
version set, and presuming that versions _between_ vulnerable versions
within a given set are also vulnerable.

1. Sort list of available versions by SemVer precedence
2. Group versions into sets based on MAJOR/MINOR versions.

       3.0.0 - 3.0.4
       2.3.0 - 2.3.107
       2.2.0 - 2.2.43
       2.1.0 - 2.1.432
       2.0.0 - 2.0.102
       1.1.0 - 1.1.102
       1.0.0 - 1.0.157
       0.1.0 - 0.1.123
       0.0.0 - 0.0.57

3. Test the highest and lowest in each MAJOR/MINOR set, and mark highest
   and lowest with known-vulnerable status.  (`(s)` means "safe" and `(v)`
   means "vulnerable".)

       3.0.0(v) - 3.0.4(s)
       2.3.0(v) - 2.3.107(s)
       2.2.0(v) - 2.2.43(v)
       2.1.0(v) - 2.1.432(v)
       2.0.0(v) - 2.0.102(v)
       1.1.0(v) - 1.1.102(s)
       1.0.0(v) - 1.0.157(v)
       0.1.0(v) - 0.1.123(v)
       0.0.0(v) - 0.0.57(v)

4. For each set of package versions:

    1. If highest and lowest both vulnerable, assume entire set is
       vulnerable, and continue to next set.  Ie, in the example, throw out
       the following version sets:

           2.2.0(v) - 2.2.43(v)
           2.1.0(v) - 2.1.432(v)
           2.0.0(v) - 2.0.102(v)
           1.0.0(v) - 1.0.157(v)
           0.1.0(v) - 0.1.123(v)
           0.0.0(v) - 0.0.57(v)

    2. Test middle version MID in set, splitting into two sets.

           3.0.0(v) - 3.0.2(v) - 3.0.4(s)
           2.3.0(v) - 2.3.54(v) - 2.3.107(s)
           1.1.0(v) - 1.1.51(v) - 1.1.102(s)

    3. If any untested versions in Set(mid..highest) or Set(lowest..mid),
       add to list of sets to test.

           3.0.0(v) - 3.0.2(v) <-- thrown out on next iteration
           3.0.2(v) - 3.0.4(s)
           2.3.0(v) - 2.3.54(v) <-- thrown out on next iteration
           2.3.54(v) - 2.3.107(s)
           1.1.0(v) - 1.1.51(v) <-- thrown out on next iteration
           1.1.51(v) - 1.1.102(s)

When the process finishes, all versions are either confirmed safe, or
confirmed/assumed vulnerable, and we avoid checking large sets of versions
where vulnerabilities went unfixed.

### Testing Version for MetaVuln Status

When the dependency is in `bundleDependencies`, we treat any dependent
version that _may_ be vulnerable as a vulnerability.  If the dependency is
not in `bundleDependencies`, then we treat the dependent module as a
vulnerability if it can _only_ resolve to dependency versions that are
vulnerable.

This relies on the reasonable assumption that the version of a bundled
dependency will be within the stated dependency range, and accounts for the
fact that we can't know ahead of time which version of a dependency may be
bundled.  So, we avoid versions that _may_ bundle a vulnerable dependency.

For example:

Package `foo` depends on package `bar` at the following version ranges:

```
foo version   bar version range
1.0.0         ^1.2.3
1.0.1         ^1.2.4
1.0.2         ^1.2.5
1.1.0         ^1.3.1
1.1.1         ^1.3.2
1.1.2         ^1.3.3
2.0.0         ^2.0.0
2.0.1         ^2.0.1
2.0.2         ^2.0.2
```

There is an advisory for `bar@1.2.4 - 1.3.2`.  So:

```
foo version   vulnerable?
1.0.0         if bundled (can use 1.2.3, which is not vulnerable)
1.0.1         yes (must use ^1.2.4, entirely contained in vuln range)
1.0.2         yes (must use ^1.2.5, entirely contained in vuln range)
1.1.0         if bundled (can use 1.3.3, which is not vulnerable)
1.1.1         if bundled (can use 1.3.3, which is not vulnerable)
1.1.2         no (dep is outside of vuln range)
2.0.0         no (dep is outside of vuln range)
2.0.1         no (dep is outside of vuln range)
2.0.2         no (dep is outside of vuln range)
```

To test a package version for metaVulnerable status, we attempt to load the
manifest of the dependency, using the vulnerable version set as the `avoid`
versions.  If we end up selecting a version that should be avoided, then
that means that the package is vulnerable by virtue of its dependency.
